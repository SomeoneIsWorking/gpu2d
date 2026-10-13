// gpu_handle — owns one SDL GPU object and releases it with the device that made it. SDL defers
// the release until the GPU has finished with the object, so dropping a handle after submitting
// work that uses it is safe.
#pragma once

#include <utility>

#include <SDL3/SDL_gpu.h>

namespace gpu2d {

template <typename T, void (*Release)(SDL_GPUDevice *, T *)> class GpuHandle {
public:
  GpuHandle() = default;
  GpuHandle(SDL_GPUDevice *device, T *object) noexcept : device_{device}, object_{object} {
  }
  ~GpuHandle() {
    reset();
  }
  GpuHandle(GpuHandle &&other) noexcept
      : device_{other.device_}, object_{std::exchange(other.object_, nullptr)} {
  }
  GpuHandle &operator=(GpuHandle &&other) noexcept {
    if (this != &other) {
      reset();
      device_ = other.device_;
      object_ = std::exchange(other.object_, nullptr);
    }
    return *this;
  }
  GpuHandle(const GpuHandle &) = delete;
  GpuHandle &operator=(const GpuHandle &) = delete;

  [[nodiscard]] T *get() const noexcept {
    return object_;
  }
  explicit operator bool() const noexcept {
    return object_ != nullptr;
  }

private:
  void reset() noexcept {
    if (object_ != nullptr) {
      Release(device_, object_);
      object_ = nullptr;
    }
  }

  SDL_GPUDevice *device_{};
  T *object_{};
};

using BufferHandle = GpuHandle<SDL_GPUBuffer, SDL_ReleaseGPUBuffer>;
using TransferBufferHandle = GpuHandle<SDL_GPUTransferBuffer, SDL_ReleaseGPUTransferBuffer>;
using TextureHandle = GpuHandle<SDL_GPUTexture, SDL_ReleaseGPUTexture>;
using SamplerHandle = GpuHandle<SDL_GPUSampler, SDL_ReleaseGPUSampler>;
using ShaderHandle = GpuHandle<SDL_GPUShader, SDL_ReleaseGPUShader>;
using PipelineHandle = GpuHandle<SDL_GPUGraphicsPipeline, SDL_ReleaseGPUGraphicsPipeline>;

} // namespace gpu2d
