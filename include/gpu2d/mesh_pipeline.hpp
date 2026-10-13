// mesh_pipeline — textured triangles with a colour per vertex: images, text, tessellated gradients
// and plain triangles (which sample a white texel).
#pragma once

#include <cstdint>

#include <SDL3/SDL_gpu.h>

#include "gpu2d/pipeline.hpp"

namespace gpu2d {

/// One vertex: position in target pixels, texture coordinate, straight-alpha colour.
struct Vertex {
  float x{};
  float y{};
  float u{};
  float v{};
  std::uint8_t r{255};
  std::uint8_t g{255};
  std::uint8_t b{255};
  std::uint8_t a{255};
};

/// The fragment stage's uniform: whether the bound texture holds premultiplied colour.
struct SamplingUniform {
  float premultiplied{};
};

class MeshPipeline {
public:
  explicit MeshPipeline(SDL_GPUDevice &device);

  /// The pipeline drawing into `format` with `blend`. Indexed triangle lists from vertex buffer
  /// slot 0, one sampler at fragment slot 0, a TargetUniform at vertex slot 0 and a
  /// SamplingUniform at fragment slot 0.
  [[nodiscard]] SDL_GPUGraphicsPipeline *get(SDL_GPUTextureFormat format, BlendMode blend) {
    return pipelines_.get(format, blend);
  }

private:
  PipelineSet pipelines_;
};

} // namespace gpu2d
