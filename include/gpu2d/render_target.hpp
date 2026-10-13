// render_target — an offscreen colour target the canvas draws into, sampled afterwards (a backdrop
// blur reads the scene from one) or read back to RGBA.
#pragma once

#include <SDL3/SDL_gpu.h>

#include "gpu2d/gpu_handle.hpp"
#include "gpu2d/image/bitmap.hpp"
#include "gpu2d/surface.hpp"
#include "gpu2d/texture.hpp"

namespace gpu2d {

class RenderTarget {
public:
  /// The pixel format every render target uses.
  static constexpr SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

  /// A `width` x `height` target; its contents are undefined until a pass clears it. Throws
  /// GpuError.
  RenderTarget(SDL_GPUDevice &device, int width, int height);

  [[nodiscard]] Surface surface() const noexcept {
    return Surface{texture_.get(), width_, height_, format};
  }
  /// The target as a texture to draw; what was drawn into it is premultiplied.
  [[nodiscard]] TextureView view(Filter filter = Filter::Linear) const noexcept {
    return TextureView{texture_.get(), width_, height_, filter, Alpha::Premultiplied};
  }
  [[nodiscard]] int width() const noexcept {
    return width_;
  }
  [[nodiscard]] int height() const noexcept {
    return height_;
  }

  /// The pixels as drawn (premultiplied RGBA, rows top first). Waits for every submission on the
  /// device before it, so call it after submitting the frame. Throws GpuError.
  [[nodiscard]] image::Bitmap read() const;

private:
  SDL_GPUDevice *device_;
  TextureHandle texture_;
  int width_;
  int height_;
};

} // namespace gpu2d
