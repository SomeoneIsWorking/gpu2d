#include "gpu2d/image/png_encoder.hpp"

#include <png.h>

namespace gpu2d::image {

std::string encodePng(std::span<const std::uint8_t> rgba, unsigned width, unsigned height,
                      std::string &error) {
  if (width == 0 || height == 0 || rgba.size() < static_cast<std::size_t>(width) * height * 4) {
    error = "no pixels to encode at " + std::to_string(width) + "x" + std::to_string(height);
    return {};
  }
  png_image image{};
  image.version = PNG_IMAGE_VERSION;
  image.width = width;
  image.height = height;
  image.format = PNG_FORMAT_RGBA;
  png_alloc_size_t size = 0;
  if (png_image_write_to_memory(&image, nullptr, &size, 0, rgba.data(), 0, nullptr) == 0) {
    error = std::string{"cannot size the PNG: "} + image.message;
    return {};
  }
  std::string bytes(size, '\0');
  if (png_image_write_to_memory(&image, bytes.data(), &size, 0, rgba.data(), 0, nullptr) == 0) {
    error = std::string{"cannot encode the PNG: "} + image.message;
    return {};
  }
  bytes.resize(size);
  return bytes;
}

} // namespace gpu2d::image
