#include "gpu2d/canvas.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <stdexcept>

#include "gpu2d/gpu_error.hpp"

namespace gpu2d {
namespace {

constexpr std::array<std::uint8_t, 4> whitePixel{255, 255, 255, 255};

void toFloats(Colour colour, float (&out)[4]) {
  out[0] = static_cast<float>(colour.r) / 255.0f;
  out[1] = static_cast<float>(colour.g) / 255.0f;
  out[2] = static_cast<float>(colour.b) / 255.0f;
  out[3] = static_cast<float>(colour.a) / 255.0f;
}

SamplerHandle makeSampler(SDL_GPUDevice &device, SDL_GPUFilter filter,
                          SDL_GPUSamplerMipmapMode mipmaps) {
  SDL_GPUSamplerCreateInfo info{};
  info.min_filter = filter;
  info.mag_filter = filter;
  info.mipmap_mode = mipmaps;
  info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  info.max_lod = 1000.0f;
  SamplerHandle sampler{&device, SDL_CreateGPUSampler(&device, &info)};
  if (!sampler) {
    throw GpuError{"cannot create a sampler"};
  }
  return sampler;
}

/// Whole pixels inside `area`, within `bounds`.
SDL_Rect inside(const Rect &area, const SDL_Rect &bounds) {
  const int left = std::max(static_cast<int>(std::ceil(area.x)), bounds.x);
  const int top = std::max(static_cast<int>(std::ceil(area.y)), bounds.y);
  const int right = std::min(static_cast<int>(std::floor(area.right())), bounds.x + bounds.w);
  const int bottom = std::min(static_cast<int>(std::floor(area.bottom())), bounds.y + bounds.h);
  return SDL_Rect{left, top, std::max(right - left, 0), std::max(bottom - top, 0)};
}

bool sameScissor(const SDL_Rect &a, const SDL_Rect &b) {
  return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

template <typename T> std::span<const std::byte> bytesOf(const std::vector<T> &items) {
  return std::as_bytes(std::span{items});
}

} // namespace

Point Transform::apply(Point point) const noexcept {
  return Point{point.x * scale + offset.x, point.y * scale + offset.y};
}

Rect Transform::apply(const Rect &rect) const noexcept {
  const Point origin = apply(Point{rect.x, rect.y});
  return Rect{origin.x, origin.y, rect.width * scale, rect.height * scale};
}

Transform Transform::after(const Transform &inner) const noexcept {
  return Transform{scale * inner.scale, apply(inner.offset)};
}

Transform Transform::translate(Point by) noexcept {
  return Transform{1.0f, by};
}

Transform Transform::scaleAbout(Point centre, float scale) noexcept {
  return Transform{scale, Point{centre.x - centre.x * scale, centre.y - centre.y * scale}};
}

Canvas::Canvas(SDL_GPUDevice &device)
    : device_{&device}, shapePipeline_{device}, meshPipeline_{device}, blurPipeline_{device},
      shapeBuffer_{device, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ},
      vertexBuffer_{device, SDL_GPU_BUFFERUSAGE_VERTEX},
      indexBuffer_{device, SDL_GPU_BUFFERUSAGE_INDEX},
      nearest_{makeSampler(device, SDL_GPU_FILTER_NEAREST, SDL_GPU_SAMPLERMIPMAPMODE_NEAREST)},
      linear_{makeSampler(device, SDL_GPU_FILTER_LINEAR, SDL_GPU_SAMPLERMIPMAPMODE_NEAREST)},
      trilinear_{makeSampler(device, SDL_GPU_FILTER_LINEAR, SDL_GPU_SAMPLERMIPMAPMODE_LINEAR)},
      white_{device, whitePixel, 1, 1, Filter::Nearest} {
}

void Canvas::begin(const Surface &target, std::optional<Colour> clear) {
  if (!transforms_.empty() || !clips_.empty()) {
    throw std::logic_error{"a canvas pass began with a transform or clip still pushed"};
  }
  passes_.push_back(Pass{target, clear, {}});
  currentPass_ = passes_.size() - 1;
}

Canvas::Pass &Canvas::current() {
  if (passes_.empty()) {
    throw std::logic_error{"a canvas draw came before any begin()"};
  }
  return passes_[currentPass_];
}

Transform Canvas::transform() const noexcept {
  return transforms_.empty() ? Transform{} : transforms_.back();
}

void Canvas::pushTransform(const Transform &transform) {
  transforms_.push_back(this->transform().after(transform));
}

void Canvas::popTransform() {
  if (transforms_.empty()) {
    throw std::logic_error{"popTransform without a pushTransform"};
  }
  transforms_.pop_back();
}

void Canvas::pushClip(const Rect &area) {
  const Surface &target = current().target;
  const SDL_Rect bounds =
      clips_.empty() ? SDL_Rect{0, 0, target.width, target.height} : clips_.back();
  clips_.push_back(inside(transform().apply(area), bounds));
}

void Canvas::popClip() {
  if (clips_.empty()) {
    throw std::logic_error{"popClip without a pushClip"};
  }
  clips_.pop_back();
}

bool Canvas::extend(BatchKind kind, const TextureView &texture, std::uint32_t first,
                    std::uint32_t count) {
  Pass &pass = current();
  const SDL_Rect scissor =
      clips_.empty() ? SDL_Rect{0, 0, pass.target.width, pass.target.height} : clips_.back();
  if (scissor.w <= 0 || scissor.h <= 0) {
    return false;
  }
  if (!pass.batches.empty()) {
    Batch &last = pass.batches.back();
    const bool mergeable = kind == BatchKind::Shapes || kind == BatchKind::Mesh;
    if (mergeable && last.kind == kind && last.blend == blend_ &&
        sameScissor(last.scissor, scissor) && last.texture == texture &&
        last.first + last.count == first) {
      last.count += count;
      return true;
    }
  }
  pass.batches.push_back(Batch{.kind = kind,
                               .blend = blend_,
                               .scissor = scissor,
                               .texture = texture,
                               .first = first,
                               .count = count});
  return true;
}

void Canvas::addShape(ShapeRecord shape) {
  const Transform t = transform();
  const Point centre = t.apply(Point{shape.bounds[0], shape.bounds[1]});
  shape.bounds[0] = centre.x;
  shape.bounds[1] = centre.y;
  shape.bounds[2] *= t.scale;
  shape.bounds[3] *= t.scale;
  shape.params[0] *= t.scale;
  shape.params[1] *= t.scale;
  if (extend(BatchKind::Shapes, TextureView{}, static_cast<std::uint32_t>(shapes_.size()), 1)) {
    shapes_.push_back(shape);
  }
}

void Canvas::roundBox(const Box &box, Colour colour) {
  const Rect &rect = box.rect;
  if (rect.width <= 0.0f || rect.height <= 0.0f || colour.a == 0) {
    return;
  }
  const float halfWidth = rect.width * 0.5f;
  const float halfHeight = rect.height * 0.5f;
  ShapeRecord shape;
  shape.bounds[0] = rect.x + halfWidth;
  shape.bounds[1] = rect.y + halfHeight;
  shape.bounds[2] = halfWidth;
  shape.bounds[3] = halfHeight;
  toFloats(colour, shape.colour);
  shape.axis[2] = ShapeRecord::roundBox;
  shape.params[0] = std::clamp(box.radius, 0.0f, std::min(halfWidth, halfHeight));
  shape.params[1] = std::max(box.stroke, 0.0f);
  addShape(shape);
}

void Canvas::fillRect(const Rect &rect, Colour colour) {
  roundBox(Box{.rect = rect, .radius = 0.0f, .stroke = 0.0f}, colour);
}

void Canvas::fillRoundRect(const Rect &rect, float radius, Colour colour) {
  roundBox(Box{.rect = rect, .radius = radius, .stroke = 0.0f}, colour);
}

void Canvas::strokeRoundRect(const Rect &rect, float radius, float width, Colour colour) {
  if (width <= 0.0f) {
    return;
  }
  roundBox(Box{.rect = rect, .radius = radius, .stroke = width}, colour);
}

void Canvas::fillCircle(Point centre, float radius, Colour colour) {
  roundBox(Box{.rect = Rect{centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f},
               .radius = radius,
               .stroke = 0.0f},
           colour);
}

void Canvas::ring(const Ring &ring, Colour colour) {
  const auto [centre, inner, outer, startDegrees, endDegrees] = ring;
  const float sweep = std::abs(endDegrees - startDegrees);
  if (outer <= inner || outer <= 0.0f || sweep <= 0.0f || colour.a == 0) {
    return;
  }
  const float middle = (startDegrees + endDegrees) * 0.5f * std::numbers::pi_v<float> / 180.0f;
  ShapeRecord shape;
  shape.bounds[0] = centre.x;
  shape.bounds[1] = centre.y;
  shape.bounds[2] = outer;
  shape.bounds[3] = outer;
  toFloats(colour, shape.colour);
  shape.axis[0] = std::cos(middle);
  shape.axis[1] = std::sin(middle);
  shape.axis[2] = ShapeRecord::arc;
  shape.params[0] = std::max(inner, 0.0f);
  shape.params[1] = outer;
  shape.params[2] = std::min(sweep, 360.0f) * 0.5f * std::numbers::pi_v<float> / 180.0f;
  addShape(shape);
}

void Canvas::line(Point from, Point to, float width, Colour colour) {
  const float dx = to.x - from.x;
  const float dy = to.y - from.y;
  const float length = std::hypot(dx, dy);
  if (length <= 0.0f || width <= 0.0f || colour.a == 0) {
    return;
  }
  ShapeRecord shape;
  shape.bounds[0] = (from.x + to.x) * 0.5f;
  shape.bounds[1] = (from.y + to.y) * 0.5f;
  shape.bounds[2] = length * 0.5f;
  shape.bounds[3] = width * 0.5f;
  toFloats(colour, shape.colour);
  shape.axis[0] = dx / length;
  shape.axis[1] = dy / length;
  shape.axis[2] = ShapeRecord::roundBox;
  addShape(shape);
}

void Canvas::triangle(Point a, Point b, Point c, Colour colour) {
  const std::array<Vertex, 3> vertices{
      Vertex{a.x, a.y, 0.0f, 0.0f, colour.r, colour.g, colour.b, colour.a},
      Vertex{b.x, b.y, 0.0f, 0.0f, colour.r, colour.g, colour.b, colour.a},
      Vertex{c.x, c.y, 0.0f, 0.0f, colour.r, colour.g, colour.b, colour.a},
  };
  constexpr std::array<std::uint32_t, 3> order{0, 1, 2};
  mesh(TextureView{}, vertices, order);
}

void Canvas::mesh(const TextureView &texture, std::span<const Vertex> vertices,
                  std::span<const std::uint32_t> indices) {
  if (vertices.empty() || indices.empty()) {
    return;
  }
  const TextureView sampled = texture.texture != nullptr ? texture : white_.view();
  const auto base = static_cast<std::uint32_t>(vertices_.size());
  if (!extend(BatchKind::Mesh, sampled, static_cast<std::uint32_t>(indices_.size()),
              static_cast<std::uint32_t>(indices.size()))) {
    return;
  }
  const Transform t = transform();
  for (Vertex vertex : vertices) {
    const Point at = t.apply(Point{vertex.x, vertex.y});
    vertex.x = at.x;
    vertex.y = at.y;
    vertices_.push_back(vertex);
  }
  for (const std::uint32_t index : indices) {
    if (index >= vertices.size()) {
      throw std::out_of_range{"a mesh index points past its vertices"};
    }
    indices_.push_back(base + index);
  }
}

void Canvas::image(const TextureView &texture, const Rect &source, const Rect &destination,
                   Colour tint) {
  if (texture.texture == nullptr || texture.width <= 0 || texture.height <= 0) {
    return;
  }
  const auto width = static_cast<float>(texture.width);
  const auto height = static_cast<float>(texture.height);
  const float u0 = source.x / width;
  const float v0 = source.y / height;
  const float u1 = source.right() / width;
  const float v1 = source.bottom() / height;
  const float x1 = destination.right();
  const float y1 = destination.bottom();
  const std::array<Vertex, 4> vertices{
      Vertex{destination.x, destination.y, u0, v0, tint.r, tint.g, tint.b, tint.a},
      Vertex{x1, destination.y, u1, v0, tint.r, tint.g, tint.b, tint.a},
      Vertex{x1, y1, u1, v1, tint.r, tint.g, tint.b, tint.a},
      Vertex{destination.x, y1, u0, v1, tint.r, tint.g, tint.b, tint.a},
  };
  constexpr std::array<std::uint32_t, 6> order{0, 1, 2, 0, 2, 3};
  mesh(texture, vertices, order);
}

RenderTarget &Canvas::strip(int width, int height) {
  if (stripsUsed_ == strips_.size()) {
    strips_.push_back(nullptr);
  }
  std::unique_ptr<RenderTarget> &slot = strips_[stripsUsed_++];
  if (slot == nullptr || slot->width() != width || slot->height() != height) {
    slot = std::make_unique<RenderTarget>(*device_, width, height);
  }
  return *slot;
}

void Canvas::blurBehind(const Backdrop &backdrop) {
  if (backdrop.scene == nullptr || backdrop.sigma <= 0.0f || backdrop.opacity <= 0.0f) {
    return;
  }
  const Rect body = transform().apply(backdrop.body);
  if (body.width <= 0.0f || body.height <= 0.0f) {
    return;
  }
  if (backdrop.scene->surface().texture == current().target.texture) {
    throw std::logic_error{"a backdrop blur cannot read the target it draws into"};
  }
  // The strip is the body heightened by three sigma each side, which the vertical pass reads
  // past the body's own rows.
  const float sigma = backdrop.sigma * transform().scale;
  const float margin = std::ceil(3.0f * sigma);
  const auto width = static_cast<int>(std::ceil(body.width));
  const auto height = static_cast<int>(std::ceil(body.height + 2.0f * margin));
  const float left = std::floor(body.x);
  const float top = std::floor(body.y - margin);
  RenderTarget &stripTarget = strip(width, height);
  const auto stripWidth = static_cast<float>(width);
  const auto stripHeight = static_cast<float>(height);
  const auto sceneWidth = static_cast<float>(backdrop.scene->width());
  const auto sceneHeight = static_cast<float>(backdrop.scene->height());

  BlurDraw draw;
  draw.source = backdrop.scene->view(Filter::Nearest);
  draw.placement = BlurPlacement{
      .destination = {0.0f, 0.0f, stripWidth, stripHeight},
      .source = {left / sceneWidth, top / sceneHeight, (left + stripWidth) / sceneWidth,
                 (top + stripHeight) / sceneHeight},
      .targetSize = {stripWidth, stripHeight},
      .padding = {},
  };
  draw.horizontal = HorizontalBlur{.texel = {1.0f / sceneWidth, 0.0f}, .sigma = sigma};
  const auto slot = static_cast<std::uint32_t>(blurs_.size());
  blurs_.push_back(draw);
  // The horizontal pass runs before the current one, into the strip.
  const Surface stripSurface = stripTarget.surface();
  passes_.insert(passes_.begin() + static_cast<std::ptrdiff_t>(currentPass_),
                 Pass{stripSurface,
                      Colour{0, 0, 0, 0},
                      {Batch{.kind = BatchKind::BlurHorizontal,
                             .blend = BlendMode::Replace,
                             .scissor = SDL_Rect{0, 0, width, height},
                             .texture = draw.source,
                             .first = slot,
                             .count = 1}}});
  ++currentPass_;

  BlurDraw vertical = draw;
  vertical.source = stripTarget.view(Filter::Nearest);
  const Surface &target = current().target;
  vertical.placement = BlurPlacement{
      .destination = {left, top, stripWidth, stripHeight},
      .source = {0.0f, 0.0f, 1.0f, 1.0f},
      .targetSize = {static_cast<float>(target.width), static_cast<float>(target.height)},
      .padding = {},
  };
  const float halfWidth = body.width * 0.5f;
  const float halfHeight = body.height * 0.5f;
  vertical.vertical = VerticalBlur{
      .texel = {0.0f, 1.0f / stripHeight},
      .sigma = sigma,
      .opacity = std::clamp(backdrop.opacity, 0.0f, 1.0f),
      .shape = {body.x + halfWidth, body.y + halfHeight, halfWidth, halfHeight},
      .radius =
          std::clamp(backdrop.radius * transform().scale, 0.0f, std::min(halfWidth, halfHeight)),
      .padding = {},
  };
  const auto verticalSlot = static_cast<std::uint32_t>(blurs_.size());
  blurs_.push_back(vertical);
  extend(BatchKind::BlurVertical, vertical.source, verticalSlot, 1);
}

SDL_GPUSampler *Canvas::sampler(Filter filter) const noexcept {
  switch (filter) {
  case Filter::Nearest:
    return nearest_.get();
  case Filter::Linear:
    return linear_.get();
  case Filter::Trilinear:
    return trilinear_.get();
  }
  return linear_.get();
}

void Canvas::submit(SDL_GPUCommandBuffer *commands) {
  if (!transforms_.empty() || !clips_.empty()) {
    throw std::logic_error{"a canvas frame ended with a transform or clip still pushed"};
  }
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
  shapeBuffer_.upload(copy, bytesOf(shapes_));
  vertexBuffer_.upload(copy, bytesOf(vertices_));
  indexBuffer_.upload(copy, bytesOf(indices_));
  SDL_EndGPUCopyPass(copy);
  for (const Pass &pass : passes_) {
    encode(commands, pass);
  }
  passes_.clear();
  currentPass_ = 0;
  shapes_.clear();
  vertices_.clear();
  indices_.clear();
  blurs_.clear();
  stripsUsed_ = 0;
  blend_ = BlendMode::Over;
}

void Canvas::encode(SDL_GPUCommandBuffer *commands, const Pass &pass) {
  SDL_GPUColorTargetInfo colour{};
  colour.texture = pass.target.texture;
  colour.store_op = SDL_GPU_STOREOP_STORE;
  colour.load_op = SDL_GPU_LOADOP_LOAD;
  if (pass.clear) {
    float clear[4]{};
    toFloats(*pass.clear, clear);
    colour.load_op = SDL_GPU_LOADOP_CLEAR;
    colour.clear_color =
        SDL_FColor{clear[0] * clear[3], clear[1] * clear[3], clear[2] * clear[3], clear[3]};
  }
  SDL_GPURenderPass *render = SDL_BeginGPURenderPass(commands, &colour, 1, nullptr);
  if (render == nullptr) {
    throw GpuError{"cannot begin a render pass"};
  }
  const TargetUniform target{
      .size = {static_cast<float>(pass.target.width), static_cast<float>(pass.target.height)}};
  const SDL_GPUTextureFormat format = pass.target.format;
  for (const Batch &batch : pass.batches) {
    SDL_SetGPUScissor(render, &batch.scissor);
    switch (batch.kind) {
    case BatchKind::Shapes: {
      SDL_BindGPUGraphicsPipeline(render, shapePipeline_.get(format, batch.blend));
      SDL_GPUBuffer *shapes = shapeBuffer_.buffer();
      SDL_BindGPUVertexStorageBuffers(render, 0, &shapes, 1);
      SDL_BindGPUFragmentStorageBuffers(render, 0, &shapes, 1);
      SDL_PushGPUVertexUniformData(commands, 0, &target, sizeof(target));
      SDL_DrawGPUPrimitives(render, 4, batch.count, 0, batch.first);
      break;
    }
    case BatchKind::Mesh: {
      SDL_BindGPUGraphicsPipeline(render, meshPipeline_.get(format, batch.blend));
      const SDL_GPUBufferBinding vertices{.buffer = vertexBuffer_.buffer(), .offset = 0};
      const SDL_GPUBufferBinding indices{.buffer = indexBuffer_.buffer(), .offset = 0};
      SDL_BindGPUVertexBuffers(render, 0, &vertices, 1);
      SDL_BindGPUIndexBuffer(render, &indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);
      const SDL_GPUTextureSamplerBinding binding{.texture = batch.texture.texture,
                                                 .sampler = sampler(batch.texture.filter)};
      SDL_BindGPUFragmentSamplers(render, 0, &binding, 1);
      const SamplingUniform sampling{.premultiplied =
                                         batch.texture.alpha == Alpha::Premultiplied ? 1.0f : 0.0f};
      SDL_PushGPUVertexUniformData(commands, 0, &target, sizeof(target));
      SDL_PushGPUFragmentUniformData(commands, 0, &sampling, sizeof(sampling));
      SDL_DrawGPUIndexedPrimitives(render, batch.count, 1, batch.first, 0, 0);
      break;
    }
    case BatchKind::BlurHorizontal:
    case BatchKind::BlurVertical: {
      const BlurDraw &blur = blurs_[batch.first];
      const bool horizontal = batch.kind == BatchKind::BlurHorizontal;
      SDL_BindGPUGraphicsPipeline(render, horizontal ? blurPipeline_.horizontal(format)
                                                     : blurPipeline_.vertical(format, batch.blend));
      const SDL_GPUTextureSamplerBinding binding{.texture = blur.source.texture,
                                                 .sampler = nearest_.get()};
      SDL_BindGPUFragmentSamplers(render, 0, &binding, 1);
      SDL_PushGPUVertexUniformData(commands, 0, &blur.placement, sizeof(blur.placement));
      if (horizontal) {
        SDL_PushGPUFragmentUniformData(commands, 0, &blur.horizontal, sizeof(blur.horizontal));
      } else {
        SDL_PushGPUFragmentUniformData(commands, 0, &blur.vertical, sizeof(blur.vertical));
      }
      SDL_DrawGPUPrimitives(render, 4, 1, 0, 0);
      break;
    }
    }
  }
  SDL_EndGPURenderPass(render);
}

} // namespace gpu2d
