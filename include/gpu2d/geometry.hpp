// geometry — the canvas's points, rectangles and colours, in target pixels with y down.
#pragma once

#include <cstdint>

namespace gpu2d {

struct Point {
  float x{};
  float y{};
};

struct Rect {
  float x{};
  float y{};
  float width{};
  float height{};

  [[nodiscard]] float right() const noexcept {
    return x + width;
  }
  [[nodiscard]] float bottom() const noexcept {
    return y + height;
  }
};

/// Straight (not premultiplied) 8-bit sRGB colour.
struct Colour {
  std::uint8_t r{};
  std::uint8_t g{};
  std::uint8_t b{};
  std::uint8_t a{255};
};

} // namespace gpu2d
