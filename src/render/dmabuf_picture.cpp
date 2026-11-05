#include "gpu2d/dmabuf_picture.hpp"

#include <array>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL3/SDL_properties.h>

#include "gpu2d/gpu_error.hpp"

namespace gpu2d {
namespace {

constexpr std::uint32_t vertexCode[] = {
#include "ycbcr.vert.inc"
};

constexpr std::string_view vertexMsl{
#ifdef GPU2D_SHADER_MSL
#include "ycbcr.vert.msl.inc"
#endif
};

constexpr std::uint32_t fragmentCode[] = {
#include "ycbcr.frag.inc"
};

constexpr std::string_view fragmentMsl{
#ifdef GPU2D_SHADER_MSL
#include "ycbcr.frag.msl.inc"
#endif
};

/// `plane` as a sampled `width` x `height` texture of `format`, its memory the DMA-BUF's.
TextureHandle wrap(SDL_GPUDevice &device, const DmabufPlane &plane, SDL_GPUTextureFormat format,
                   int width, int height) {
#ifdef SDL_PROP_GPU_TEXTURE_CREATE_VULKAN_DMABUF_FD_NUMBER
  const SDL_PropertiesID props = SDL_CreateProperties();
  SDL_SetNumberProperty(props, SDL_PROP_GPU_TEXTURE_CREATE_VULKAN_DMABUF_FD_NUMBER, plane.fd);
  SDL_SetNumberProperty(props, SDL_PROP_GPU_TEXTURE_CREATE_VULKAN_DMABUF_OFFSET_NUMBER,
                        static_cast<Sint64>(plane.offset));
  SDL_SetNumberProperty(props, SDL_PROP_GPU_TEXTURE_CREATE_VULKAN_DMABUF_PITCH_NUMBER,
                        static_cast<Sint64>(plane.pitch));
  SDL_SetNumberProperty(props, SDL_PROP_GPU_TEXTURE_CREATE_VULKAN_DMABUF_MODIFIER_NUMBER,
                        static_cast<Sint64>(plane.modifier));
  SDL_GPUTextureCreateInfo info{};
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.format = format;
  info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  info.width = static_cast<Uint32>(width);
  info.height = static_cast<Uint32>(height);
  info.layer_count_or_depth = 1;
  info.num_levels = 1;
  info.props = props;
  TextureHandle texture{&device, SDL_CreateGPUTexture(&device, &info)};
  SDL_DestroyProperties(props);
  if (!texture) {
    throw GpuError{"cannot wrap a DMA-BUF plane"};
  }
  return texture;
#else
  static_cast<void>(device);
  static_cast<void>(plane);
  static_cast<void>(format);
  static_cast<void>(width);
  static_cast<void>(height);
  throw GpuError{"this SDL wraps no DMA-BUF"};
#endif
}

SamplerHandle nearestSampler(SDL_GPUDevice &device) {
  SDL_GPUSamplerCreateInfo info{};
  info.min_filter = SDL_GPU_FILTER_NEAREST;
  info.mag_filter = SDL_GPU_FILTER_NEAREST;
  info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
  info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  SamplerHandle sampler{&device, SDL_CreateGPUSampler(&device, &info)};
  if (!sampler) {
    throw GpuError{"cannot create a picture's sampler"};
  }
  return sampler;
}

} // namespace

DmabufPicture::DmabufPicture(SDL_GPUDevice &device)
    : device_{&device},
      pipeline_{
          device,
          ShaderStage{.spirv = vertexCode, .msl = vertexMsl, .stage = SDL_GPU_SHADERSTAGE_VERTEX},
          ShaderStage{.spirv = fragmentCode,
                      .msl = fragmentMsl,
                      .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
                      .samplers = 2,
                      .uniformBuffers = 1},
          VertexInput{
              .primitive = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP, .buffers = {}, .attributes = {}}},
      sampler_{nearestSampler(device)} {
}

DmabufPicture::~DmabufPicture() {
  std::vector<SDL_GPUFence *> fences;
  for (const Reading &reading : reading_) {
    fences.push_back(reading.fence.get());
  }
  if (!fences.empty()) {
    SDL_WaitForGPUFences(device_, true, fences.data(), static_cast<Uint32>(fences.size()));
  }
}

void DmabufPicture::retire() {
  while (!reading_.empty() && SDL_QueryGPUFence(device_, reading_.front().fence.get())) {
    reading_.pop_front();
  }
}

void DmabufPicture::show(DmabufYcbcr picture, const YcbcrToRgb &colour) {
  retire();
  if (picture.width <= 0 || picture.height <= 0 || picture.width % 2 != 0 ||
      picture.height % 2 != 0) {
    throw std::invalid_argument{"a 4:2:0 picture needs a positive, even size"};
  }
  const TextureHandle luma =
      wrap(*device_, picture.luma, SDL_GPU_TEXTUREFORMAT_R8_UNORM, picture.width, picture.height);
  const TextureHandle chroma = wrap(*device_, picture.chroma, SDL_GPU_TEXTUREFORMAT_R8G8_UNORM,
                                    picture.width / 2, picture.height / 2);
  if (!target_ || target_->width() != picture.width || target_->height() != picture.height) {
    target_.emplace(*device_, picture.width, picture.height);
  }
  SDL_GPUGraphicsPipeline *pipeline = pipeline_.get(RenderTarget::format, BlendMode::Replace);

  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device_);
  if (commands == nullptr) {
    throw GpuError{"cannot record a picture's draw"};
  }
  SDL_GPUColorTargetInfo into{};
  into.texture = target_->surface().texture;
  into.load_op = SDL_GPU_LOADOP_DONT_CARE;
  into.store_op = SDL_GPU_STOREOP_STORE;
  // The picture before may still be drawn from; SDL gives this one a texture of its own.
  into.cycle = true;
  SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(commands, &into, 1, nullptr);
  if (pass == nullptr) {
    SDL_CancelGPUCommandBuffer(commands);
    throw GpuError{"cannot begin a picture's draw"};
  }
  SDL_BindGPUGraphicsPipeline(pass, pipeline);
  const std::array samplers{
      SDL_GPUTextureSamplerBinding{.texture = luma.get(), .sampler = sampler_.get()},
      SDL_GPUTextureSamplerBinding{.texture = chroma.get(), .sampler = sampler_.get()}};
  SDL_BindGPUFragmentSamplers(pass, 0, samplers.data(), static_cast<Uint32>(samplers.size()));
  SDL_PushGPUFragmentUniformData(commands, 0, &colour, sizeof colour);
  SDL_DrawGPUPrimitives(pass, 4, 1, 0, 0);
  SDL_EndGPURenderPass(pass);
  SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
  if (fence == nullptr) {
    throw GpuError{"cannot submit a picture's draw"};
  }
  reading_.push_back(
      Reading{.fence = FenceHandle{device_, fence}, .owner = std::move(picture.owner)});
}

std::optional<TextureView> DmabufPicture::view(Filter filter) const noexcept {
  if (!target_) {
    return std::nullopt;
  }
  return target_->view(filter);
}

image::Bitmap DmabufPicture::read() const {
  if (!target_) {
    return {};
  }
  return target_->read();
}

} // namespace gpu2d
