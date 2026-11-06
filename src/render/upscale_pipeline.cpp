#include "gpu2d/upscale_pipeline.hpp"

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

constexpr std::uint32_t fragmentCode[] = {
#include "upscale.frag.inc"
};

constexpr std::string_view fragmentMsl{
#ifdef GPU2D_SHADER_MSL
#include "upscale.frag.msl.inc"
#endif
};

constexpr ShaderStage vertexStage{.spirv = vertexCode,
                                  .msl = vertexMsl,
                                  .stage = SDL_GPU_SHADERSTAGE_VERTEX,
                                  .uniformBuffers = 1};

constexpr ShaderStage fragmentStage{.spirv = fragmentCode,
                                    .msl = fragmentMsl,
                                    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
                                    .samplers = 1};

} // namespace

UpscalePipeline::UpscalePipeline(SDL_GPUDevice &device)
    : pipelines_{device, vertexStage, fragmentStage,
                 VertexInput{.primitive = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP,
                             .buffers = {},
                             .attributes = {}}} {
}

} // namespace gpu2d
