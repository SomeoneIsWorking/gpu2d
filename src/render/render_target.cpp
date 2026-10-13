#include "gpu2d/render_target.hpp"

#include <cstring>
#include <stdexcept>

#include "gpu2d/gpu_error.hpp"

namespace gpu2d {

RenderTarget::RenderTarget(SDL_GPUDevice &device, int width, int height)
    : device_{&device}, width_{width}, height_{height} {
  if (width <= 0 || height <= 0) {
    throw std::invalid_argument{"a render target needs a positive size"};
  }
  SDL_GPUTextureCreateInfo info{};
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.format = format;
  info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
  info.width = static_cast<Uint32>(width);
  info.height = static_cast<Uint32>(height);
  info.layer_count_or_depth = 1;
  info.num_levels = 1;
  texture_ = TextureHandle{&device, SDL_CreateGPUTexture(&device, &info)};
  if (!texture_) {
    throw GpuError{"cannot create a render target"};
  }
}

image::Bitmap RenderTarget::read() const {
  const auto bytes = static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * 4;
  const SDL_GPUTransferBufferCreateInfo info{.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
                                             .size = static_cast<Uint32>(bytes),
                                             .props = 0};
  const TransferBufferHandle download{device_, SDL_CreateGPUTransferBuffer(device_, &info)};
  if (!download) {
    throw GpuError{"cannot create a read-back buffer"};
  }
  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device_);
  if (commands == nullptr) {
    throw GpuError{"cannot record a read-back"};
  }
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
  SDL_GPUTextureRegion from{};
  from.texture = texture_.get();
  from.w = static_cast<Uint32>(width_);
  from.h = static_cast<Uint32>(height_);
  from.d = 1;
  const SDL_GPUTextureTransferInfo to{.transfer_buffer = download.get(),
                                      .offset = 0,
                                      .pixels_per_row = static_cast<Uint32>(width_),
                                      .rows_per_layer = static_cast<Uint32>(height_)};
  SDL_DownloadFromGPUTexture(copy, &from, &to);
  SDL_EndGPUCopyPass(copy);
  SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
  if (fence == nullptr) {
    throw GpuError{"cannot submit a read-back"};
  }
  const bool waited = SDL_WaitForGPUFences(device_, true, &fence, 1);
  SDL_ReleaseGPUFence(device_, fence);
  if (!waited) {
    throw GpuError{"cannot wait for a read-back"};
  }
  const void *mapped = SDL_MapGPUTransferBuffer(device_, download.get(), false);
  if (mapped == nullptr) {
    throw GpuError{"cannot map a read-back"};
  }
  image::Bitmap pixels{.width = width_, .height = height_, .rgba = {}};
  pixels.rgba.resize(bytes);
  std::memcpy(pixels.rgba.data(), mapped, bytes);
  SDL_UnmapGPUTransferBuffer(device_, download.get());
  return pixels;
}

} // namespace gpu2d
