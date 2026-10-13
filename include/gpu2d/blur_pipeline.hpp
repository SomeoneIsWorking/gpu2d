// blur_pipeline — the two passes of a backdrop blur: a horizontal Gaussian
// from the scene into a strip, then a vertical one from the strip onto the target through a rounded
// rectangle's mask.
#pragma once

#include <SDL3/SDL_gpu.h>

#include "gpu2d/pipeline.hpp"

namespace gpu2d {

/// The vertex stage's uniform for both passes (std140).
struct BlurPlacement {
  /// Where the quad lands, in target pixels: x, y, width, height.
  float destination[4]{};
  /// The texture coordinates it reads: left, top, right, bottom.
  float source[4]{};
  float targetSize[2]{};
  float padding[2]{};
};

/// The horizontal pass's fragment uniform (std140).
struct HorizontalBlur {
  float texel[2]{};
  float sigma{};
  float padding{};
};

/// The vertical pass's fragment uniform (std140).
struct VerticalBlur {
  float texel[2]{};
  float sigma{};
  float opacity{};
  /// The mask's centre and half extent in target pixels.
  float shape[4]{};
  float radius{};
  float padding[3]{};
};

class BlurPipeline {
public:
  explicit BlurPipeline(SDL_GPUDevice &device);

  /// Both draw 4 vertices with a sampler at fragment slot 0, a BlurPlacement at vertex slot 0
  /// and their own uniform at fragment slot 0.
  [[nodiscard]] SDL_GPUGraphicsPipeline *horizontal(SDL_GPUTextureFormat format) {
    return horizontal_.get(format, BlendMode::Replace);
  }
  [[nodiscard]] SDL_GPUGraphicsPipeline *vertical(SDL_GPUTextureFormat format, BlendMode blend) {
    return vertical_.get(format, blend);
  }

private:
  PipelineSet horizontal_;
  PipelineSet vertical_;
};

} // namespace gpu2d
