#include "gpu2d/blur_pipeline.hpp"

#include <cstdint>
#include <string_view>

namespace gpu2d {
namespace {

constexpr std::uint32_t vertexCode[] = {
#include "blur.vert.inc"
};

constexpr std::string_view vertexMsl{
#ifdef GPU2D_SHADER_MSL
#include "blur.vert.msl.inc"
#endif
};

constexpr std::uint32_t horizontalCode[] = {
#include "blur_horizontal.frag.inc"
};

constexpr std::string_view horizontalMsl{
#ifdef GPU2D_SHADER_MSL
#include "blur_horizontal.frag.msl.inc"
#endif
};

constexpr std::uint32_t verticalCode[] = {
#include "blur_vertical.frag.inc"
};

constexpr std::string_view verticalMsl{
#ifdef GPU2D_SHADER_MSL
#include "blur_vertical.frag.msl.inc"
#endif
};

constexpr ShaderStage vertexStage{.spirv = vertexCode,
                                  .msl = vertexMsl,
                                  .stage = SDL_GPU_SHADERSTAGE_VERTEX,
                                  .uniformBuffers = 1};

ShaderStage fragmentStage(std::span<const std::uint32_t> spirv, std::string_view msl) {
  return ShaderStage{.spirv = spirv,
                     .msl = msl,
                     .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
                     .samplers = 1,
                     .uniformBuffers = 1};
}

} // namespace

BlurPipeline::BlurPipeline(SDL_GPUDevice &device)
    : horizontal_{device, vertexStage, fragmentStage(horizontalCode, horizontalMsl),
                  VertexInput{.primitive = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP,
                              .buffers = {},
                              .attributes = {}}},
      vertical_{device, vertexStage, fragmentStage(verticalCode, verticalMsl),
                VertexInput{.primitive = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP,
                            .buffers = {},
                            .attributes = {}}} {
}

} // namespace gpu2d
