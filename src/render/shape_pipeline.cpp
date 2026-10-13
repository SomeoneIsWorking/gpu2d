#include "gpu2d/shape_pipeline.hpp"

#include <cstdint>

namespace gpu2d {
namespace {

constexpr std::uint32_t vertexCode[] = {
#include "shape.vert.inc"
};

constexpr std::uint32_t fragmentCode[] = {
#include "shape.frag.inc"
};

static_assert(sizeof(ShapeRecord) == 64, "ShapeRecord must match shaders/shape.glsl");

} // namespace

ShapePipeline::ShapePipeline(SDL_GPUDevice &device)
    : pipelines_{
          device,
          ShaderStage{.spirv = vertexCode,
                      .stage = SDL_GPU_SHADERSTAGE_VERTEX,
                      .storageBuffers = 1,
                      .uniformBuffers = 1},
          ShaderStage{
              .spirv = fragmentCode, .stage = SDL_GPU_SHADERSTAGE_FRAGMENT, .storageBuffers = 1},
          VertexInput{
              .primitive = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP, .buffers = {}, .attributes = {}}} {
}

} // namespace gpu2d
