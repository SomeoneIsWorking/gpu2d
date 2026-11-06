// upscale_pipeline — FSR 1's edge-adaptive upscale, one pass from a texture onto the target.
#pragma once

#include <SDL3/SDL_gpu.h>

#include "gpu2d/pipeline.hpp"

namespace gpu2d {

class UpscalePipeline {
public:
  explicit UpscalePipeline(SDL_GPUDevice &device);

  /// Draws 4 vertices with a sampler at fragment slot 0 and a BlurPlacement at vertex slot 0.
  [[nodiscard]] SDL_GPUGraphicsPipeline *get(SDL_GPUTextureFormat format) {
    return pipelines_.get(format, BlendMode::Replace);
  }

private:
  PipelineSet pipelines_;
};

} // namespace gpu2d
