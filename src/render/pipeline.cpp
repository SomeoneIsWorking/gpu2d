#include "gpu2d/pipeline.hpp"

#include <algorithm>
#include <utility>

#include "gpu2d/gpu_error.hpp"

namespace gpu2d {
namespace {

ShaderHandle makeShader(SDL_GPUDevice &device, const ShaderStage &stage) {
  SDL_GPUShaderCreateInfo info{};
  const SDL_GPUShaderFormat taken = SDL_GetGPUShaderFormats(&device);
  if ((taken & SDL_GPU_SHADERFORMAT_SPIRV) != 0 && !stage.spirv.empty()) {
    info.code = reinterpret_cast<const Uint8 *>(stage.spirv.data());
    info.code_size = stage.spirv.size_bytes();
    info.entrypoint = "main";
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
  } else if ((taken & SDL_GPU_SHADERFORMAT_MSL) != 0 && !stage.msl.empty()) {
    info.code = reinterpret_cast<const Uint8 *>(stage.msl.data());
    info.code_size = stage.msl.size();
    // SPIR-V's `main` is a reserved name in MSL.
    info.entrypoint = "main0";
    info.format = SDL_GPU_SHADERFORMAT_MSL;
  } else {
    throw GpuError{"the device takes neither SPIR-V nor this build's MSL"};
  }
  info.stage = stage.stage;
  info.num_samplers = stage.samplers;
  info.num_storage_buffers = stage.storageBuffers;
  info.num_uniform_buffers = stage.uniformBuffers;
  ShaderHandle shader{&device, SDL_CreateGPUShader(&device, &info)};
  if (!shader) {
    throw GpuError{"cannot create a shader"};
  }
  return shader;
}

SDL_GPUColorTargetBlendState blendState(BlendMode blend) {
  SDL_GPUColorTargetBlendState state{};
  if (blend == BlendMode::Replace) {
    return state;
  }
  state.enable_blend = true;
  state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
  state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  state.color_blend_op = SDL_GPU_BLENDOP_ADD;
  state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
  state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
  return state;
}

} // namespace

SDL_GPUShaderFormat builtShaderFormats() noexcept {
#ifdef GPU2D_SHADER_MSL
  return SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL;
#else
  return SDL_GPU_SHADERFORMAT_SPIRV;
#endif
}

PipelineSet::PipelineSet(SDL_GPUDevice &device, const ShaderStage &vertex,
                         const ShaderStage &fragment, VertexInput input)
    : device_{&device}, vertex_{makeShader(device, vertex)},
      fragment_{makeShader(device, fragment)}, primitive_{input.primitive},
      buffers_{input.buffers.begin(), input.buffers.end()},
      attributes_{input.attributes.begin(), input.attributes.end()} {
}

SDL_GPUGraphicsPipeline *PipelineSet::get(SDL_GPUTextureFormat format, BlendMode blend) {
  const auto found = std::ranges::find_if(pipelines_, [format, blend](const Entry &entry) {
    return entry.format == format && entry.blend == blend;
  });
  if (found != pipelines_.end()) {
    return found->pipeline.get();
  }
  SDL_GPUColorTargetDescription target{};
  target.format = format;
  target.blend_state = blendState(blend);
  SDL_GPUGraphicsPipelineCreateInfo info{};
  info.vertex_shader = vertex_.get();
  info.fragment_shader = fragment_.get();
  info.primitive_type = primitive_;
  info.vertex_input_state.vertex_buffer_descriptions = buffers_.data();
  info.vertex_input_state.num_vertex_buffers = static_cast<Uint32>(buffers_.size());
  info.vertex_input_state.vertex_attributes = attributes_.data();
  info.vertex_input_state.num_vertex_attributes = static_cast<Uint32>(attributes_.size());
  info.target_info.color_target_descriptions = &target;
  info.target_info.num_color_targets = 1;
  PipelineHandle pipeline{device_, SDL_CreateGPUGraphicsPipeline(device_, &info)};
  if (!pipeline) {
    throw GpuError{"cannot create a graphics pipeline"};
  }
  pipelines_.push_back(Entry{.format = format, .blend = blend, .pipeline = std::move(pipeline)});
  return pipelines_.back().pipeline.get();
}

} // namespace gpu2d
