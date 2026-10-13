// png_encoder — RGBA pixels as PNG bytes in memory, through libpng.
#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace gpu2d::image {

/// Rows of `width` RGBA pixels, top first, as a PNG file's bytes. Empty when the pixels do not
/// fill the size or libpng fails, with the reason in `error`.
[[nodiscard]] std::string encodePng(std::span<const std::uint8_t> rgba, unsigned width,
                                    unsigned height, std::string &error);

} // namespace gpu2d::image
