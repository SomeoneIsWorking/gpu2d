#include "gpu2d/texture.hpp"

#include <algorithm>
#include <bit>
#include <cstring>
#include <stdexcept>

#include "gpu2d/gpu_error.hpp"

namespace gpu2d {
namespace {

std::uint32_t levelsFor(int width, int height, Filter filter) {
  if (filter != Filter::Trilinear) {
    return 1;
  }
  return static_cast<std::uint32_t>(std::bit_width(static_cast<unsigned>(std::max(width, height))));
}

} // namespace

Texture::Texture(SDL_GPUDevice &device, std::span<const std::uint8_t> rgba, int width, int height,
                 Filter filter)
    : width_{width}, height_{height}, filter_{filter} {
  const auto bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
  if (width <= 0 || height <= 0 || rgba.size() < bytes) {
    throw std::invalid_argument{"a texture needs width x height RGBA pixels"};
  }
  const std::uint32_t levels = levelsFor(width, height, filter);
  SDL_GPUTextureCreateInfo info{};
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  // Building mip levels renders into them.
  info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER |
               (levels > 1 ? SDL_GPU_TEXTUREUSAGE_COLOR_TARGET : SDL_GPUTextureUsageFlags{0});
  info.width = static_cast<Uint32>(width);
  info.height = static_cast<Uint32>(height);
  info.layer_count_or_depth = 1;
  info.num_levels = levels;
  texture_ = TextureHandle{&device, SDL_CreateGPUTexture(&device, &info)};
  if (!texture_) {
    throw GpuError{"cannot create a texture"};
  }

  const SDL_GPUTransferBufferCreateInfo staging{
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = static_cast<Uint32>(bytes), .props = 0};
  const TransferBufferHandle transfer{&device, SDL_CreateGPUTransferBuffer(&device, &staging)};
  if (!transfer) {
    throw GpuError{"cannot create a texture's staging buffer"};
  }
  void *mapped = SDL_MapGPUTransferBuffer(&device, transfer.get(), false);
  if (mapped == nullptr) {
    throw GpuError{"cannot map a texture's staging buffer"};
  }
  std::memcpy(mapped, rgba.data(), bytes);
  SDL_UnmapGPUTransferBuffer(&device, transfer.get());

  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(&device);
  if (commands == nullptr) {
    throw GpuError{"cannot record a texture upload"};
  }
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
  const SDL_GPUTextureTransferInfo from{.transfer_buffer = transfer.get(),
                                        .offset = 0,
                                        .pixels_per_row = static_cast<Uint32>(width),
                                        .rows_per_layer = static_cast<Uint32>(height)};
  SDL_GPUTextureRegion to{};
  to.texture = texture_.get();
  to.w = static_cast<Uint32>(width);
  to.h = static_cast<Uint32>(height);
  to.d = 1;
  SDL_UploadToGPUTexture(copy, &from, &to, false);
  SDL_EndGPUCopyPass(copy);
  if (levels > 1) {
    SDL_GenerateMipmapsForGPUTexture(commands, texture_.get());
  }
  if (!SDL_SubmitGPUCommandBuffer(commands)) {
    throw GpuError{"cannot submit a texture upload"};
  }
}

} // namespace gpu2d
