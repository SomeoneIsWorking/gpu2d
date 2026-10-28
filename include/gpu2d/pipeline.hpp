// pipeline — shaders from embedded SPIR-V or MSL, whichever the device takes, and the graphics
// pipelines of one shader pair, made per target format and blend mode on first use (SDL GPU bakes
// both into a pipeline).
#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "gpu2d/gpu_handle.hpp"

namespace gpu2d {

/// How a draw combines with the target. Colours reaching the blender are premultiplied.
enum class BlendMode : std::uint8_t {
  /// Source over destination, alpha included, so the target's alpha is what a compositor
  /// blends the window with.
  Over,
  /// The source replaces the destination.
  Replace,
};

/// The shader formats gpu2d's own shaders are built in: SPIR-V, and MSL where built with it.
[[nodiscard]] SDL_GPUShaderFormat builtShaderFormats() noexcept;

/// One shader stage and the resources it declares (SDL GPU needs the counts up front).
struct ShaderStage {
  std::span<const std::uint32_t> spirv;
  /// The same stage in MSL, entry point `main0`; empty in a build without MSL.
  std::string_view msl;
  SDL_GPUShaderStage stage{SDL_GPU_SHADERSTAGE_VERTEX};
  std::uint32_t samplers{};
  std::uint32_t storageBuffers{};
  std::uint32_t uniformBuffers{};
};

/// What a pipeline's vertex stage reads and how it assembles primitives.
struct VertexInput {
  SDL_GPUPrimitiveType primitive{SDL_GPU_PRIMITIVETYPE_TRIANGLELIST};
  std::span<const SDL_GPUVertexBufferDescription> buffers;
  std::span<const SDL_GPUVertexAttribute> attributes;
};

class PipelineSet {
public:
  /// Compiles both stages in the first of SPIR-V and MSL the device takes; throws GpuError when
  /// the driver refuses them or takes neither.
  PipelineSet(SDL_GPUDevice &device, const ShaderStage &vertex, const ShaderStage &fragment,
              VertexInput input);

  /// The pipeline for drawing into `format` with `blend`; throws GpuError when it cannot be made.
  [[nodiscard]] SDL_GPUGraphicsPipeline *get(SDL_GPUTextureFormat format, BlendMode blend);

private:
  struct Entry {
    SDL_GPUTextureFormat format{};
    BlendMode blend{};
    PipelineHandle pipeline;
  };

  SDL_GPUDevice *device_;
  ShaderHandle vertex_;
  ShaderHandle fragment_;
  SDL_GPUPrimitiveType primitive_;
  std::vector<SDL_GPUVertexBufferDescription> buffers_;
  std::vector<SDL_GPUVertexAttribute> attributes_;
  std::vector<Entry> pipelines_;
};

} // namespace gpu2d
