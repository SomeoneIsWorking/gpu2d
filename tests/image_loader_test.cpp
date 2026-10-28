// The image loader on a real device behind a hidden window: a PNG decodes on a worker and draws
// once a frame uploads it, the same image is decoded once, a dropped handle cancels its decode,
// a failure is reported with its reason, and a frame uploads no more than its budget.
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <future>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "gpu2d/geometry.hpp"
#include "gpu2d/gpu_window.hpp"
#include "gpu2d/image/png_encoder.hpp"
#include "gpu2d/image_loader.hpp"
#include "gpu_fixture.hpp"
#include "scratch_dir.hpp"

namespace {

using gpu2d::Filter;
using gpu2d::ImageHandle;
using gpu2d::ImageLoader;
using gpu2d::ImageRequest;
using gpu2d::ImageState;
using gpu2d::image::Bitmap;
using gpu2d::test::expect;

constexpr std::size_t everything = std::numeric_limits<std::size_t>::max();

Bitmap solid(gpu2d::PixelSize size, gpu2d::Colour colour) {
  Bitmap bitmap{.width = size.width, .height = size.height, .rgba = {}};
  for (int i = 0; i < size.width * size.height; ++i) {
    bitmap.rgba.insert(bitmap.rgba.end(), {colour.r, colour.g, colour.b, colour.a});
  }
  return bitmap;
}

std::filesystem::path writePng(const std::filesystem::path &dir, const char *name,
                               const Bitmap &bitmap) {
  std::string error;
  const std::string png = gpu2d::image::encodePng(bitmap.rgba, static_cast<unsigned>(bitmap.width),
                                                  static_cast<unsigned>(bitmap.height), error);
  expect(!png.empty(), "the fixture encodes");
  const std::filesystem::path file = dir / name;
  std::ofstream{file, std::ios::binary} << png;
  return file;
}

void landsOnUploadAndDraws(gpu2d::test::GpuFixture &gpu, const std::filesystem::path &dir) {
  const std::filesystem::path file = writePng(dir, "green.png", solid({16, 16}, {0, 255, 0, 255}));
  ImageLoader loader;
  const ImageHandle handle = loader.loadFile(file, Filter::Nearest);
  expect(handle.pending(), "a request is pending at once");
  expect(handle.texture() == nullptr, "no texture before it lands");
  loader.waitAll();
  expect(handle.pending() && !loader.idle(), "decoded but not uploaded is still pending");
  const gpu2d::ImageUploads uploads = loader.upload(gpu.device(), everything);
  expect(uploads.textures == 1 && uploads.failures.empty(), "the upload lands one texture");
  expect(handle.state() == ImageState::Ready && loader.idle(), "the handle reads ready");
  const gpu2d::Texture *texture = handle.texture();
  expect(texture != nullptr && texture->width() == 16 && texture->height() == 16,
         "the texture has the file's size");
  const Bitmap frame = gpu.draw([texture](gpu2d::Canvas &canvas) {
    canvas.image(texture->view(), gpu2d::Rect{0.0f, 0.0f, 16.0f, 16.0f},
                 gpu2d::Rect{0.0f, 0.0f, 64.0f, 64.0f}, gpu2d::Colour{255, 255, 255, 255});
  });
  gpu2d::test::expectPixel(frame, 32, 32, {0, 255, 0, 255}, "the landed texture draws");
}

void scalesDownToMaxSide(gpu2d::test::GpuFixture &gpu, const std::filesystem::path &dir) {
  const std::filesystem::path file = writePng(dir, "tall.png", solid({60, 120}, {9, 9, 9, 255}));
  ImageLoader loader;
  const ImageHandle handle = loader.loadFile(file, Filter::Linear, 40);
  loader.waitAll();
  static_cast<void>(loader.upload(gpu.device(), everything));
  expect(handle.texture() != nullptr && handle.texture()->width() == 20 &&
             handle.texture()->height() == 40,
         "a file larger than the limit is scaled to fit, keeping its aspect");
}

void sharesOneDecode(gpu2d::test::GpuFixture &gpu) {
  std::atomic<int> decodes{0};
  const auto request = [&decodes](Filter filter) {
    return ImageRequest{.key = "shared",
                        .decode = [&decodes](std::string &) -> std::optional<Bitmap> {
                          ++decodes;
                          return solid({4, 4}, {1, 2, 3, 255});
                        },
                        .filter = filter};
  };
  ImageLoader loader;
  const ImageHandle first = loader.load(request(Filter::Linear));
  const ImageHandle second = loader.load(request(Filter::Linear));
  const ImageHandle other = loader.load(request(Filter::Nearest));
  loader.waitAll();
  static_cast<void>(loader.upload(gpu.device(), everything));
  expect(decodes == 2, "the same image and filter decode once; another filter is its own");
  expect(first.texture() != nullptr && first.texture() == second.texture(),
         "requests for the same image share its texture");
  expect(other.texture() != first.texture(), "another filter is another texture");
}

void droppedHandleCancels(gpu2d::test::GpuFixture &gpu) {
  std::promise<void> gate;
  std::shared_future<void> open = gate.get_future().share();
  std::atomic<int> cancelledRan{0};
  ImageLoader loader{1};
  // Holds the only worker, so the next request stays queued.
  const ImageHandle blocker =
      loader.load(ImageRequest{.key = "blocker",
                               .decode = [open](std::string &) -> std::optional<Bitmap> {
                                 open.wait();
                                 return solid({2, 2}, {0, 0, 0, 255});
                               },
                               .filter = Filter::Linear});
  {
    const ImageHandle dropped =
        loader.load(ImageRequest{.key = "dropped",
                                 .decode = [&cancelledRan](std::string &) -> std::optional<Bitmap> {
                                   ++cancelledRan;
                                   return solid({2, 2}, {0, 0, 0, 255});
                                 },
                                 .filter = Filter::Linear});
  }
  gate.set_value();
  loader.waitAll();
  expect(cancelledRan == 0, "an image whose handles are gone is never decoded");
  expect(loader.upload(gpu.device(), everything).textures == 1, "only the held image lands");
  expect(blocker.texture() != nullptr, "the held image lands");
}

void reportsFailures(gpu2d::test::GpuFixture &gpu, const std::filesystem::path &dir) {
  ImageLoader loader;
  const ImageHandle missing = loader.loadFile(dir / "missing.png", Filter::Linear);
  const ImageHandle thrown =
      loader.load(ImageRequest{.key = "thrown",
                               .decode = [](std::string &) -> std::optional<Bitmap> {
                                 throw std::runtime_error{"the decoder threw"};
                               },
                               .filter = Filter::Linear});
  loader.waitAll();
  expect(missing.state() == ImageState::Failed && !missing.error().empty(),
         "a missing file fails with SDL_image's reason");
  expect(thrown.error() == "the decoder threw", "a decoder's exception is the failure");
  const gpu2d::ImageUploads uploads = loader.upload(gpu.device(), everything);
  expect(uploads.failures.size() == 2 && uploads.textures == 0,
         "every failure is handed to the next upload");
  expect(loader.upload(gpu.device(), everything).failures.empty(), "a failure is handed once");
}

void keepsToTheBudget(gpu2d::test::GpuFixture &gpu) {
  ImageLoader loader;
  std::vector<ImageHandle> handles;
  handles.reserve(3);
  for (int i = 0; i < 3; ++i) {
    handles.push_back(
        loader.load(ImageRequest{.key = "budget" + std::to_string(i),
                                 .decode = [](std::string &) -> std::optional<Bitmap> {
                                   return solid({8, 8}, {5, 5, 5, 255});
                                 },
                                 .filter = Filter::Linear}));
  }
  loader.waitAll();
  const std::size_t one = std::size_t{8} * 8 * 4;
  expect(loader.upload(gpu.device(), 0).textures == 1, "a frame always lands one image");
  expect(loader.upload(gpu.device(), one * 2).textures == 2, "a frame lands what fits");
  expect(loader.idle(), "nothing is left");
}

void waitTakesAheadOfTheQueue(gpu2d::test::GpuFixture &gpu) {
  std::promise<void> gate;
  std::shared_future<void> open = gate.get_future().share();
  ImageLoader loader{1};
  const auto request = [open](std::string key) {
    return ImageRequest{.key = std::move(key),
                        .decode = [open](std::string &) -> std::optional<Bitmap> {
                          open.wait();
                          return solid({2, 2}, {0, 0, 0, 255});
                        },
                        .filter = Filter::Linear};
  };
  const ImageHandle first = loader.load(request("first"));
  const ImageHandle second = loader.load(request("second"));
  const ImageHandle wanted = loader.load(request("wanted"));
  gate.set_value();
  loader.wait(wanted);
  static_cast<void>(loader.upload(gpu.device(), everything));
  expect(wanted.texture() != nullptr, "the image waited for has landed");
  loader.waitAll();
}

} // namespace

int main() {
  const gpu2d::test::ScratchDir dir{GPU2D_TEST_SCRATCH, "image_loader"};
  gpu2d::test::GpuFixture gpu;
  landsOnUploadAndDraws(gpu, dir.path());
  scalesDownToMaxSide(gpu, dir.path());
  sharesOneDecode(gpu);
  droppedHandleCancels(gpu);
  reportsFailures(gpu, dir.path());
  keepsToTheBudget(gpu);
  waitTakesAheadOfTheQueue(gpu);
  std::printf("image_loader: all checks passed\n");
  return 0;
}
