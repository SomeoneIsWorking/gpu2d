// Images through SDL3_image and libpng: the PNG encoder's bytes decode to the same pixels, JPEG and
// WebP decode, garbage does not, and resizing keeps the picture.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>
#include <webp/encode.h>

#include "gpu2d/image/decode.hpp"
#include "gpu2d/image/png_encoder.hpp"
#include "gpu2d/image/resize.hpp"
#include "scratch_dir.hpp"
#include "check.hpp"

namespace {

using gpu2d::image::Bitmap;
using gpu2d::test::expect;
using gpu2d::test::need;

constexpr int side = 8;

/// Left half red, right half blue, the bottom row half transparent.
std::vector<std::uint8_t> picture() {
  std::vector<std::uint8_t> rgba;
  for (int y = 0; y < side; ++y) {
    for (int x = 0; x < side; ++x) {
      const bool left = x < side / 2;
      rgba.insert(rgba.end(), {static_cast<std::uint8_t>(left ? 255 : 0), 0,
                               static_cast<std::uint8_t>(left ? 0 : 255),
                               static_cast<std::uint8_t>(y == side - 1 ? 128 : 255)});
    }
  }
  return rgba;
}

std::span<const std::byte> bytes(const std::string &data) {
  return std::as_bytes(std::span{data});
}

bool within(std::uint8_t a, std::uint8_t b, int tolerance) {
  return a - b <= tolerance && b - a <= tolerance;
}

void pngRoundTrip() {
  const std::vector<std::uint8_t> rgba = picture();
  std::string error;
  const std::string png = gpu2d::image::encodePng(rgba, side, side, error);
  expect(png.compare(0, 8, "\x89PNG\r\n\x1a\n") == 0, "the encoder writes a PNG signature");
  const Bitmap decoded = need(gpu2d::image::decode(bytes(png), error), "the PNG decodes");
  expect(decoded.width == side && decoded.height == side, "the PNG keeps its size");
  expect(decoded.rgba == rgba, "the PNG keeps every pixel, alpha included");
  expect(gpu2d::image::encodePng(rgba, side, side + 1, error).empty(),
         "too few pixels for the size encode nothing");
  expect(!error.empty(), "a refused encode says why");
}

void webpDecodes() {
  const std::vector<std::uint8_t> rgba = picture();
  std::uint8_t *encoded = nullptr;
  const std::size_t size = WebPEncodeLosslessRGBA(rgba.data(), side, side, side * 4, &encoded);
  expect(size > 0, "libwebp encodes the fixture");
  const std::string webp{reinterpret_cast<const char *>(encoded), size};
  WebPFree(encoded);
  std::string error;
  const Bitmap decoded = need(gpu2d::image::decode(bytes(webp), error), "the WebP decodes");
  expect(decoded.width == side && decoded.rgba == rgba, "a lossless WebP keeps every pixel");
}

void jpegDecodesFromFile(const std::filesystem::path &dir) {
  const std::vector<std::uint8_t> rgba = picture();
  SDL_Surface *surface = SDL_CreateSurfaceFrom(side, side, SDL_PIXELFORMAT_RGBA32,
                                               const_cast<std::uint8_t *>(rgba.data()), side * 4);
  expect(surface != nullptr, "the fixture wraps as a surface");
  const std::filesystem::path file = dir / "cover.jpg";
  const bool saved = IMG_SaveJPG(surface, file.string().c_str(), 95);
  SDL_DestroySurface(surface);
  expect(saved, "SDL3_image writes the JPEG fixture");
  std::string error;
  const Bitmap decoded = need(gpu2d::image::decodeFile(file, error), "the JPEG decodes");
  expect(decoded.width == side && decoded.height == side, "the JPEG keeps its size");
  const std::size_t red = decoded.offset(1, 1);
  const std::size_t blue = decoded.offset(side - 2, 1);
  expect(within(decoded.rgba[red], 255, 24) && within(decoded.rgba[red + 2], 0, 24),
         "the JPEG's left is red");
  expect(within(decoded.rgba[blue], 0, 24) && within(decoded.rgba[blue + 2], 255, 24),
         "the JPEG's right is blue");
  expect(decoded.rgba[red + 3] == 255, "a JPEG decodes opaque");
}

void refusesGarbage(const std::filesystem::path &dir) {
  std::string error;
  expect(!gpu2d::image::decode(bytes("not an image"), error), "garbage decodes to nothing");
  expect(error.starts_with("cannot decode"), "a garbage decode says why");
  error.clear();
  expect(!gpu2d::image::decodeFile(dir / "missing.png", error),
         "a missing file decodes to nothing");
  expect(error.starts_with("cannot open"), "a missing file says why");
}

void resizes() {
  const Bitmap source{.width = side, .height = side, .rgba = picture()};
  const Bitmap half = gpu2d::image::resized(source, side / 2, side / 4);
  expect(half.width == side / 2 && half.height == side / 4, "resizing gives the asked size");
  expect(half.rgba[0] > 200 && half.rgba[2] < 50, "the left stays red");
  const std::size_t right = half.offset(side / 2 - 1, 0);
  expect(half.rgba[right] < 50 && half.rgba[right + 2] > 200, "the right stays blue");
  expect(gpu2d::image::resized(source, 0, 4).empty(), "a zero size gives nothing");

  const Bitmap wide{.width = 40, .height = 10, .rgba = std::vector<std::uint8_t>(1600, 255)};
  const Bitmap fitted = gpu2d::image::fittedWithin(wide, 20);
  expect(fitted.width == 20 && fitted.height == 5, "fitting scales the longest side down");
  expect(gpu2d::image::fittedWithin(wide, 64).width == 40, "a fitting image is unchanged");
}

} // namespace

int main() {
  const gpu2d::test::ScratchDir scratch{GPU2D_TEST_SCRATCH, "image"};
  pngRoundTrip();
  webpDecodes();
  jpegDecodesFromFile(scratch.path());
  refusesGarbage(scratch.path());
  resizes();
  std::printf("image: all checks passed\n");
  return 0;
}
