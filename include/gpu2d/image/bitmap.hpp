// bitmap — RGBA pixels, four bytes a pixel, rows top first, no padding. Decoded images are
// straight alpha; a render target read back is premultiplied.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gpu2d::image {

struct Bitmap {
  int width{};
  int height{};
  std::vector<std::uint8_t> rgba;

  [[nodiscard]] bool empty() const noexcept {
    return width <= 0 || height <= 0 || rgba.empty();
  }
  /// The byte offset of pixel (x, y).
  [[nodiscard]] std::size_t offset(int x, int y) const noexcept {
    return (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
            static_cast<std::size_t>(x)) *
           4;
  }
};

} // namespace gpu2d::image
