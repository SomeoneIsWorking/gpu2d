// canvas — the 2D drawing API. Draws are recorded per frame into passes, one per
// target, and merged into as few GPU draws as their state allows: consecutive shapes share one
// instanced draw, consecutive triangles on one texture one indexed draw. `submit` uploads the
// frame's data once and encodes every pass.
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "gpu2d/blur_pipeline.hpp"
#include "gpu2d/geometry.hpp"
#include "gpu2d/gpu_handle.hpp"
#include "gpu2d/mesh_pipeline.hpp"
#include "gpu2d/pipeline.hpp"
#include "gpu2d/render_target.hpp"
#include "gpu2d/shape_pipeline.hpp"
#include "gpu2d/stream_buffer.hpp"
#include "gpu2d/surface.hpp"
#include "gpu2d/texture.hpp"

namespace gpu2d {

/// A uniform scale about the origin, then an offset: p' = p * scale + offset.
struct Transform {
  float scale{1.0f};
  Point offset{};

  [[nodiscard]] Point apply(Point point) const noexcept;
  [[nodiscard]] Rect apply(const Rect &rect) const noexcept;
  /// `inner` first, then this.
  [[nodiscard]] Transform after(const Transform &inner) const noexcept;

  [[nodiscard]] static Transform translate(Point by) noexcept;
  /// Scales by `scale` about `centre`, which stays put.
  [[nodiscard]] static Transform scaleAbout(Point centre, float scale) noexcept;
};

/// A backdrop blur: what `scene` holds behind `body`, blurred by a Gaussian of `sigma`
/// pixels and shown through `body` with corners of `radius`.
struct Backdrop {
  const RenderTarget *scene{};
  Rect body;
  float radius{};
  float sigma{};
  float opacity{1.0f};
};

/// The ring between `inner` and `outer` from `startDegrees` to `endDegrees`, clockwise on screen
/// from +x; a sweep of 360 or more is the whole ring.
struct Ring {
  Point centre;
  float inner{};
  float outer{};
  float startDegrees{};
  float endDegrees{};
};

class Canvas {
public:
  /// Builds the pipelines; throws GpuError.
  explicit Canvas(SDL_GPUDevice &device);

  /// Starts a pass into `target`, cleared to `clear` or drawn over its contents. Every draw
  /// until the next `begin` lands there. The transform and clip stacks must be empty.
  void begin(const Surface &target, std::optional<Colour> clear);

  /// Draws through `transform` (in the coordinates of whatever is pushed already) until popped.
  void pushTransform(const Transform &transform);
  void popTransform();
  /// Clips to `area` (whole pixels inside it) within whatever is clipped already, until popped.
  /// The area is in drawing coordinates, so it follows the transform.
  void pushClip(const Rect &area);
  void popClip();
  void setBlend(BlendMode blend) noexcept {
    blend_ = blend;
  }

  void fillRect(const Rect &rect, Colour colour);
  void fillRoundRect(const Rect &rect, float radius, Colour colour);
  /// The band `width` wide inside the rounded rectangle's edge.
  void strokeRoundRect(const Rect &rect, float radius, float width, Colour colour);
  void fillCircle(Point centre, float radius, Colour colour);
  void ring(const Ring &ring, Colour colour);
  /// A bar `width` wide from `from` to `to`, with square ends.
  void line(Point from, Point to, float width, Colour colour);
  void triangle(Point a, Point b, Point c, Colour colour);
  /// Indexed triangles sampling `texture` (nullptr texture: plain colour), positions in drawing
  /// coordinates.
  void mesh(const TextureView &texture, std::span<const Vertex> vertices,
            std::span<const std::uint32_t> indices);
  /// The `source` pixels of `texture` stretched over `destination`, multiplied by `tint`.
  void image(const TextureView &texture, const Rect &source, const Rect &destination, Colour tint);
  /// The backdrop blur (two passes). `backdrop.scene` must be finished: drawn in an earlier pass
  /// than the current one.
  void blurBehind(const Backdrop &backdrop);

  /// Encodes the frame into `commands` and starts the next. The caller submits `commands`.
  void submit(SDL_GPUCommandBuffer *commands);

private:
  enum class BatchKind : std::uint8_t { Shapes, Mesh, BlurHorizontal, BlurVertical };

  /// A run of draws with one state. `first` and `count` are shape instances, mesh indices or
  /// a blur's slot.
  struct Batch {
    BatchKind kind{};
    BlendMode blend{};
    SDL_Rect scissor{};
    TextureView texture;
    std::uint32_t first{};
    std::uint32_t count{};
  };

  struct Pass {
    Surface target;
    std::optional<Colour> clear;
    std::vector<Batch> batches;
  };

  struct BlurDraw {
    TextureView source;
    BlurPlacement placement;
    HorizontalBlur horizontal;
    VerticalBlur vertical;
  };

  [[nodiscard]] Pass &current();
  [[nodiscard]] Transform transform() const noexcept;
  void addShape(ShapeRecord shape);
  /// A rounded box; a positive `stroke` keeps only the band that wide inside its edge.
  struct Box {
    Rect rect;
    float radius{};
    float stroke{};
  };

  void roundBox(const Box &box, Colour colour);
  /// Appends to the last batch when its state matches, else starts one; false when clipped out.
  bool extend(BatchKind kind, const TextureView &texture, std::uint32_t first, std::uint32_t count);
  [[nodiscard]] RenderTarget &strip(int width, int height);
  [[nodiscard]] SDL_GPUSampler *sampler(Filter filter) const noexcept;
  void encode(SDL_GPUCommandBuffer *commands, const Pass &pass);

  SDL_GPUDevice *device_;
  ShapePipeline shapePipeline_;
  MeshPipeline meshPipeline_;
  BlurPipeline blurPipeline_;
  StreamBuffer shapeBuffer_;
  StreamBuffer vertexBuffer_;
  StreamBuffer indexBuffer_;
  SamplerHandle nearest_;
  SamplerHandle linear_;
  SamplerHandle trilinear_;
  /// Sampled by plain-colour triangles.
  Texture white_;

  std::vector<Pass> passes_;
  std::size_t currentPass_{};
  std::vector<Transform> transforms_;
  std::vector<SDL_Rect> clips_;
  BlendMode blend_{BlendMode::Over};
  std::vector<ShapeRecord> shapes_;
  std::vector<Vertex> vertices_;
  std::vector<std::uint32_t> indices_;
  std::vector<BlurDraw> blurs_;
  /// The blur's intermediate strips, reused frame to frame.
  std::vector<std::unique_ptr<RenderTarget>> strips_;
  std::size_t stripsUsed_{};
};

} // namespace gpu2d
