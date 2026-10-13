#include "gpu2d/image/decode.hpp"

#include <cstring>
#include <memory>
#include <string>

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>

namespace gpu2d::image {
namespace {

struct SurfaceFree {
  void operator()(SDL_Surface *surface) const noexcept {
    SDL_DestroySurface(surface);
  }
};
using Surface = std::unique_ptr<SDL_Surface, SurfaceFree>;

/// Takes ownership of `stream`; `what` names the source in a failure.
std::optional<Bitmap> decodeStream(SDL_IOStream *stream, const std::string &what,
                                   std::string &error) {
  if (stream == nullptr) {
    error = "cannot open " + what + ": " + SDL_GetError();
    return std::nullopt;
  }
  const Surface loaded{IMG_Load_IO(stream, true)};
  if (loaded == nullptr) {
    error = "cannot decode " + what + ": " + SDL_GetError();
    return std::nullopt;
  }
  // RGBA32 is R, G, B, A in memory whatever the byte order.
  const Surface rgba{SDL_ConvertSurface(loaded.get(), SDL_PIXELFORMAT_RGBA32)};
  if (rgba == nullptr) {
    error = "cannot convert " + what + ": " + SDL_GetError();
    return std::nullopt;
  }
  Bitmap bitmap{.width = rgba->w, .height = rgba->h, .rgba = {}};
  const auto row = static_cast<std::size_t>(rgba->w) * 4;
  bitmap.rgba.resize(row * static_cast<std::size_t>(rgba->h));
  const auto *source = static_cast<const std::uint8_t *>(rgba->pixels);
  for (int y = 0; y < rgba->h; ++y) {
    std::memcpy(bitmap.rgba.data() + row * static_cast<std::size_t>(y),
                source + static_cast<std::ptrdiff_t>(y) * rgba->pitch, row);
  }
  return bitmap;
}

} // namespace

std::optional<Bitmap> decodeFile(const std::filesystem::path &file, std::string &error) {
  const std::string name = file.string();
  return decodeStream(SDL_IOFromFile(name.c_str(), "rb"), name, error);
}

std::optional<Bitmap> decode(std::span<const std::byte> bytes, std::string &error) {
  return decodeStream(SDL_IOFromConstMem(bytes.data(), bytes.size()),
                      std::to_string(bytes.size()) + " bytes in memory", error);
}

} // namespace gpu2d::image
