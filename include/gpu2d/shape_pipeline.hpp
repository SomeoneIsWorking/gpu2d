// shape_pipeline — every filled or stroked shape as a signed distance field: one instanced quad per
// shape, its parameters in a storage buffer (SDL GPU uniform pushes past 4 KB read as zero).
#pragma once

#include <SDL3/SDL_gpu.h>

#include "gpu2d/pipeline.hpp"

namespace gpu2d {

/// One shape as shaders/shape.glsl reads it: std430, four vec4s.
struct ShapeRecord {
  /// The kinds, as the shader compares them.
  static constexpr float roundBox = 0.0f;
  static constexpr float arc = 1.0f;

  /// Centre in target pixels, then the half extent of the shape's frame.
  float bounds[4]{};
  /// Straight-alpha colour, 0 to 1.
  float colour[4]{};
  /// The frame's unit x axis, then the kind.
  float axis[4]{1.0f, 0.0f, 0.0f, 0.0f};
  /// RoundBox: corner radius, stroke width (0 fills). Arc: inner and outer radius, half sweep in
  /// radians.
  float params[4]{};
};

/// The vertex stage's uniform: the target's size in pixels.
struct TargetUniform {
  float size[2]{};
};

class ShapePipeline {
public:
  explicit ShapePipeline(SDL_GPUDevice &device);

  /// The pipeline drawing into `format` with `blend`. Draw 4 vertices per instance, with the
  /// shape buffer bound to the vertex and fragment stages' storage slot 0 and a TargetUniform
  /// pushed to vertex slot 0.
  [[nodiscard]] SDL_GPUGraphicsPipeline *get(SDL_GPUTextureFormat format, BlendMode blend) {
    return pipelines_.get(format, blend);
  }

private:
  PipelineSet pipelines_;
};

} // namespace gpu2d
