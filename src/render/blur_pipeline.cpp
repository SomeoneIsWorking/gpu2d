#include "gpu2d/blur_pipeline.hpp"

#include <cstdint>

namespace gpu2d {
namespace {

constexpr std::uint32_t vertexCode[] = {
#include "blur.vert.inc"
};

constexpr std::uint32_t horizontalCode[] = {
#include "blur_horizontal.frag.inc"
};

constexpr std::uint32_t verticalCode[] = {
#include "blur_vertical.frag.inc"
};

constexpr ShaderStage vertexStage{
    .spirv = vertexCode, .stage = SDL_GPU_SHADERSTAGE_VERTEX, .uniformBuffers = 1};

ShaderStage fragmentStage(std::span<const std::uint32_t> code) {
  return ShaderStage{
      .spirv = code, .stage = SDL_GPU_SHADERSTAGE_FRAGMENT, .samplers = 1, .uniformBuffers = 1};
}

} // namespace

BlurPipeline::BlurPipeline(SDL_GPUDevice &device)
    : horizontal_{device, vertexStage, fragmentStage(horizontalCode),
                  VertexInput{.primitive = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP,
                              .buffers = {},
                              .attributes = {}}},
      vertical_{device, vertexStage, fragmentStage(verticalCode),
                VertexInput{.primitive = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP,
                            .buffers = {},
                            .attributes = {}}} {
}

} // namespace gpu2d
