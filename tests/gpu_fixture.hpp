// A hidden GPU window (unmapped, never shown), a canvas and an offscreen target to draw a test
// frame into and read back.
#pragma once

#include <cstdint>
#include <cstdio>
#include <functional>

#include <SDL3/SDL_gpu.h>

#include "gpu2d/image/bitmap.hpp"
#include "gpu2d/canvas.hpp"
#include "gpu2d/gpu_window.hpp"
#include "gpu2d/render_target.hpp"
#include "check.hpp"

namespace gpu2d::test {

struct Pixel {
  std::uint8_t r{};
  std::uint8_t g{};
  std::uint8_t b{};
  std::uint8_t a{};
};

inline Pixel at(const image::Bitmap &frame, int x, int y) {
  const std::size_t i = frame.offset(x, y);
  return Pixel{frame.rgba[i], frame.rgba[i + 1], frame.rgba[i + 2], frame.rgba[i + 3]};
}

/// Fails unless every channel of (x, y) is within `tolerance` of `wanted` (premultiplied).
inline void expectPixel(const image::Bitmap &frame, int x, int y, Pixel wanted, const char *what,
                        int tolerance = 2) {
  const Pixel got = at(frame, x, y);
  const auto off = [tolerance](int a, int b) {
    return a - b > tolerance || b - a > tolerance;
  };
  if (off(got.r, wanted.r) || off(got.g, wanted.g) || off(got.b, wanted.b) ||
      off(got.a, wanted.a)) {
    std::fprintf(stderr, "FAIL: %s: (%d, %d) is %d %d %d %d, want %d %d %d %d\n", what, x, y, got.r,
                 got.g, got.b, got.a, wanted.r, wanted.g, wanted.b, wanted.a);
    std::exit(1);
  }
}

class GpuFixture {
public:
  static constexpr int size = 64;

  GpuFixture()
      : window_{
            WindowOptions{.title = "gpu2d test", .width = size, .height = size, .hidden = true}},
        canvas_{window_.device()}, target_{window_.device(), size, size} {
  }

  [[nodiscard]] GpuWindow &window() noexcept {
    return window_;
  }
  [[nodiscard]] Canvas &canvas() noexcept {
    return canvas_;
  }
  [[nodiscard]] SDL_GPUDevice &device() noexcept {
    return window_.device();
  }

  /// Submits what `record` drew (it begins its own passes) and reads `target` back.
  image::Bitmap submit(const RenderTarget &target) {
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(&device());
    expect(commands != nullptr, "a command buffer is acquired");
    canvas_.submit(commands);
    expect(SDL_SubmitGPUCommandBuffer(commands), "the frame is submitted");
    return target.read();
  }

  /// One frame into the fixture's target, cleared to `clear` first.
  image::Bitmap draw(const std::function<void(Canvas &)> &record,
                     Colour clear = Colour{0, 0, 0, 0}) {
    canvas_.begin(target_.surface(), clear);
    record(canvas_);
    return submit(target_);
  }

private:
  GpuWindow window_;
  Canvas canvas_;
  RenderTarget target_;
};

} // namespace gpu2d::test
