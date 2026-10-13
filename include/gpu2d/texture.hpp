// texture — GPU textures from RGBA pixels, uploaded through a transfer buffer, and the view the
// canvas samples any texture through (an image, a render target or the text atlas).
#pragma once

#include <cstdint>
#include <span>

#include <SDL3/SDL_gpu.h>

#include "gpu2d/gpu_handle.hpp"

namespace gpu2d {

/// How a texture is sampled.
enum class Filter : std::uint8_t {
  Nearest,
  Linear,
  /// Linear between mip levels, which the texture builds on upload; for art drawn far smaller
  /// than its pixels.
  Trilinear,
};

/// Whether a texture's colour is already multiplied by its alpha.
enum class Alpha : std::uint8_t {
  Straight,
  Premultiplied,
};

/// A texture the canvas can draw, borrowed: its owner keeps it alive until the frame is submitted.
struct TextureView {
  SDL_GPUTexture *texture{};
  int width{};
  int height{};
  Filter filter{Filter::Linear};
  Alpha alpha{Alpha::Straight};

  [[nodiscard]] bool operator==(const TextureView &) const = default;
};

class Texture {
public:
  /// Uploads `rgba` (straight alpha, rows top first, `width` x `height`) and returns once the
  /// upload is submitted; later submissions on the device see it. Throws GpuError.
  Texture(SDL_GPUDevice &device, std::span<const std::uint8_t> rgba, int width, int height,
          Filter filter);

  [[nodiscard]] TextureView view() const noexcept {
    return TextureView{texture_.get(), width_, height_, filter_, Alpha::Straight};
  }
  [[nodiscard]] int width() const noexcept {
    return width_;
  }
  [[nodiscard]] int height() const noexcept {
    return height_;
  }

private:
  TextureHandle texture_;
  int width_;
  int height_;
  Filter filter_;
};

} // namespace gpu2d
