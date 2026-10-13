#include "gpu2d/image/resize.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

#include <SDL3/SDL_surface.h>

namespace gpu2d::image {
namespace {

struct SurfaceFree {
  void operator()(SDL_Surface *surface) const noexcept {
    SDL_DestroySurface(surface);
  }
};

} // namespace

Bitmap resized(const Bitmap &source, int width, int height) {
  if (source.empty() || width <= 0 || height <= 0) {
    return {};
  }
  // SDL only reads the borrowed pixels, though the API takes them as writable.
  const std::unique_ptr<SDL_Surface, SurfaceFree> view{
      SDL_CreateSurfaceFrom(source.width, source.height, SDL_PIXELFORMAT_RGBA32,
                            const_cast<std::uint8_t *>(source.rgba.data()), source.width * 4)};
  if (view == nullptr) {
    return {};
  }
  const std::unique_ptr<SDL_Surface, SurfaceFree> scaled{
      SDL_ScaleSurface(view.get(), width, height, SDL_SCALEMODE_LINEAR)};
  if (scaled == nullptr) {
    return {};
  }
  Bitmap out{.width = width, .height = height, .rgba = {}};
  const auto row = static_cast<std::size_t>(width) * 4;
  out.rgba.resize(row * static_cast<std::size_t>(height));
  const auto *pixels = static_cast<const std::uint8_t *>(scaled->pixels);
  for (int y = 0; y < height; ++y) {
    std::memcpy(out.rgba.data() + row * static_cast<std::size_t>(y),
                pixels + static_cast<std::ptrdiff_t>(y) * scaled->pitch, row);
  }
  return out;
}

Bitmap fittedWithin(Bitmap source, int longest) {
  const int side = std::max(source.width, source.height);
  if (source.empty() || longest <= 0 || side <= longest) {
    return source;
  }
  const float scale = static_cast<float>(longest) / static_cast<float>(side);
  const auto scaledSide = [scale](int length) {
    return std::max(1, static_cast<int>(static_cast<float>(length) * scale));
  };
  return resized(source, scaledSide(source.width), scaledSide(source.height));
}

} // namespace gpu2d::image
