#include "gpu2d/stream_buffer.hpp"

#include <algorithm>
#include <bit>
#include <cstring>
#include <utility>

#include "gpu2d/gpu_error.hpp"

namespace gpu2d {
namespace {

constexpr std::size_t smallest = 4096;

} // namespace

StreamBuffer::StreamBuffer(SDL_GPUDevice &device, SDL_GPUBufferUsageFlags usage)
    : device_{&device}, usage_{usage} {
}

void StreamBuffer::reserve(std::size_t bytes) {
  if (bytes <= capacity_) {
    return;
  }
  const std::size_t size = std::bit_ceil(std::max(bytes, smallest));
  const SDL_GPUBufferCreateInfo info{
      .usage = usage_, .size = static_cast<Uint32>(size), .props = 0};
  BufferHandle buffer{device_, SDL_CreateGPUBuffer(device_, &info)};
  const SDL_GPUTransferBufferCreateInfo staging{
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = static_cast<Uint32>(size), .props = 0};
  TransferBufferHandle transfer{device_, SDL_CreateGPUTransferBuffer(device_, &staging)};
  if (!buffer || !transfer) {
    throw GpuError{"cannot grow a frame buffer"};
  }
  buffer_ = std::move(buffer);
  transfer_ = std::move(transfer);
  capacity_ = size;
}

void StreamBuffer::upload(SDL_GPUCopyPass *copy, std::span<const std::byte> bytes) {
  if (bytes.empty()) {
    return;
  }
  reserve(bytes.size());
  void *mapped = SDL_MapGPUTransferBuffer(device_, transfer_.get(), true);
  if (mapped == nullptr) {
    throw GpuError{"cannot map a frame buffer"};
  }
  std::memcpy(mapped, bytes.data(), bytes.size());
  SDL_UnmapGPUTransferBuffer(device_, transfer_.get());
  const SDL_GPUTransferBufferLocation from{.transfer_buffer = transfer_.get(), .offset = 0};
  const SDL_GPUBufferRegion to{
      .buffer = buffer_.get(), .offset = 0, .size = static_cast<Uint32>(bytes.size())};
  SDL_UploadToGPUBuffer(copy, &from, &to, true);
}

} // namespace gpu2d
