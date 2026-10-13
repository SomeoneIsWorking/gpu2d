// decode — PNG, JPEG and WebP (and whatever else SDL3_image reads) into a Bitmap. Needs no window
// or GPU, so it runs on worker threads.
#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>

#include "gpu2d/image/bitmap.hpp"

namespace gpu2d::image {

/// The image in `file`, or nothing when it is missing or undecodable, with the reason in `error`.
[[nodiscard]] std::optional<Bitmap> decodeFile(const std::filesystem::path &file,
                                               std::string &error);

/// The image encoded in `bytes`, or nothing when undecodable, with the reason in `error`.
[[nodiscard]] std::optional<Bitmap> decode(std::span<const std::byte> bytes, std::string &error);

} // namespace gpu2d::image
