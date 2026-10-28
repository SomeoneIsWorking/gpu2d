#include "gpu2d/shape_pipeline.hpp"

#include <cstdint>
#include <string_view>

namespace gpu2d {
namespace {

constexpr std::uint32_t vertexCode[] = {
#include "shape.vert.inc"
};

constexpr std::string_view vertexMsl{
#ifdef GPU2D_SHADER_MSL
#include "shape.vert.msl.inc"
#endif
};

constexpr std::uint32_t fragmentCode[] = {
#include "shape.frag.inc"
};

constexpr std::string_view fragmentMsl{
#ifdef GPU2D_SHADER_MSL
#include "shape.frag.msl.inc"
#endif
};

static_assert(sizeof(ShapeRecord) == 64, "ShapeRecord must match shaders/shape.glsl");

} // namespace

ShapePipeline::ShapePipeline(SDL_GPUDevice &device)
    : pipelines_{device,
                 ShaderStage{.spirv = vertexCode,
                             .msl = vertexMsl,
                             .stage = SDL_GPU_SHADERSTAGE_VERTEX,
                             .storageBuffers = 1,
                             .uniformBuffers = 1},
                 ShaderStage{.spirv = fragmentCode,
                             .msl = fragmentMsl,
                             .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
                             .storageBuffers = 1},
                 VertexInput{.primitive = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP,
                             .buffers = {},
                             .attributes = {}}} {
}

} // namespace gpu2d
