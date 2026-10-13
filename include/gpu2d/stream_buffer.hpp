// stream_buffer — a GPU buffer refilled every frame from the CPU, growing to the largest frame
// seen. Each upload cycles the buffers, so a frame still in flight keeps the data it read.
#pragma once

#include <cstddef>
#include <span>

#include <SDL3/SDL_gpu.h>

#include "gpu2d/gpu_handle.hpp"

namespace gpu2d {

class StreamBuffer {
public:
  StreamBuffer(SDL_GPUDevice &device, SDL_GPUBufferUsageFlags usage);

  /// Copies `bytes` into the buffer within `copy`, growing it first if needed. Throws GpuError.
  void upload(SDL_GPUCopyPass *copy, std::span<const std::byte> bytes);

  [[nodiscard]] SDL_GPUBuffer *buffer() const noexcept {
    return buffer_.get();
  }

private:
  void reserve(std::size_t bytes);

  SDL_GPUDevice *device_;
  SDL_GPUBufferUsageFlags usage_;
  BufferHandle buffer_;
  TransferBufferHandle transfer_;
  std::size_t capacity_{};
};

} // namespace gpu2d
