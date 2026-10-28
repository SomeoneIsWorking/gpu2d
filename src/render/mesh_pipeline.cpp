#include "gpu2d/mesh_pipeline.hpp"

#include <array>
#include <cstddef>
#include <string_view>

namespace gpu2d {
namespace {

constexpr std::uint32_t vertexCode[] = {
#include "mesh.vert.inc"
};

constexpr std::string_view vertexMsl{
#ifdef GPU2D_SHADER_MSL
#include "mesh.vert.msl.inc"
#endif
};

constexpr std::uint32_t fragmentCode[] = {
#include "mesh.frag.inc"
};

constexpr std::string_view fragmentMsl{
#ifdef GPU2D_SHADER_MSL
#include "mesh.frag.msl.inc"
#endif
};

constexpr std::array<SDL_GPUVertexBufferDescription, 1> buffers{SDL_GPUVertexBufferDescription{
    .slot = 0,
    .pitch = sizeof(Vertex),
    .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    .instance_step_rate = 0,
}};

constexpr std::array<SDL_GPUVertexAttribute, 3> attributes{
    SDL_GPUVertexAttribute{.location = 0,
                           .buffer_slot = 0,
                           .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                           .offset = offsetof(Vertex, x)},
    SDL_GPUVertexAttribute{.location = 1,
                           .buffer_slot = 0,
                           .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                           .offset = offsetof(Vertex, u)},
    SDL_GPUVertexAttribute{.location = 2,
                           .buffer_slot = 0,
                           .format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
                           .offset = offsetof(Vertex, r)},
};

} // namespace

MeshPipeline::MeshPipeline(SDL_GPUDevice &device)
    : pipelines_{device,
                 ShaderStage{.spirv = vertexCode,
                             .msl = vertexMsl,
                             .stage = SDL_GPU_SHADERSTAGE_VERTEX,
                             .uniformBuffers = 1},
                 ShaderStage{.spirv = fragmentCode,
                             .msl = fragmentMsl,
                             .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
                             .samplers = 1,
                             .uniformBuffers = 1},
                 VertexInput{.primitive = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
                             .buffers = buffers,
                             .attributes = attributes}} {
}

} // namespace gpu2d
