// image_loader — images decoded on worker threads and made textures on the render thread, a
// frame's budget at a time. A request answers at once with a handle that reads pending until its
// texture lands; requests for the same image share one decode and one texture; an image no handle
// holds any more is not decoded or uploaded; a failed decode is reported, never dropped.
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "gpu2d/image/bitmap.hpp"
#include "gpu2d/texture.hpp"

namespace gpu2d {

/// Decodes one image on a worker: the pixels (straight alpha), or nothing with the reason in
/// `error`. It may throw; the exception's message is the failure.
using ImageDecode = std::function<std::optional<image::Bitmap>(std::string &error)>;

struct ImageRequest {
  /// Names the image: requests with the same key and filter share one decode and one texture.
  std::string key;
  ImageDecode decode;
  Filter filter{Filter::Linear};
};

/// A request whose decode failed, by its key.
struct ImageFailure {
  std::string key;
  std::string error;
};

/// What one `ImageLoader::upload` did.
struct ImageUploads {
  std::size_t textures{};
  /// Failures since the previous upload.
  std::vector<ImageFailure> failures;
};

enum class ImageState : std::uint8_t {
  /// A default handle, which asked for nothing.
  Empty,
  /// Queued, decoding, or decoded and waiting for an upload.
  Pending,
  Ready,
  Failed,
};

class ImageLoader {
  /// One image, shared by its handles; the loader's queues hold it weakly, so the last handle
  /// dropped cancels it.
  struct Entry {
    std::string key;
    ImageDecode decode;
    Filter filter{Filter::Linear};
    std::atomic<ImageState> state{ImageState::Pending};
    /// The rest are the worker's until the entry is decoded, then the render thread's.
    std::string error;
    image::Bitmap pixels;
    std::optional<Texture> texture;
    /// Guarded by the loader's mutex.
    bool decoded{false};
  };

public:
  /// A requested image; read it on the render thread. Copies share the image.
  class Handle {
  public:
    Handle() = default;

    [[nodiscard]] ImageState state() const noexcept {
      return entry_ ? entry_->state.load(std::memory_order_acquire) : ImageState::Empty;
    }
    [[nodiscard]] bool pending() const noexcept {
      return state() == ImageState::Pending;
    }
    /// The texture once it has landed; null before, and for a failure.
    [[nodiscard]] const Texture *texture() const noexcept {
      return state() == ImageState::Ready && entry_->texture.has_value() ? &*entry_->texture
                                                                         : nullptr;
    }
    /// Why the decode failed; empty unless it did.
    [[nodiscard]] std::string_view error() const noexcept {
      return state() == ImageState::Failed ? std::string_view{entry_->error} : std::string_view{};
    }

  private:
    friend class ImageLoader;
    explicit Handle(std::shared_ptr<Entry> entry) noexcept : entry_{std::move(entry)} {
    }

    std::shared_ptr<Entry> entry_;
  };

  static constexpr std::size_t defaultWorkers = 2;

  explicit ImageLoader(std::size_t workers = defaultWorkers);
  ~ImageLoader();
  ImageLoader(const ImageLoader &) = delete;
  ImageLoader &operator=(const ImageLoader &) = delete;
  ImageLoader(ImageLoader &&) = delete;
  ImageLoader &operator=(ImageLoader &&) = delete;

  /// Queues `request` unless a handle already holds the same image, which it shares.
  [[nodiscard]] Handle load(ImageRequest request);
  /// The image in `file` through SDL3_image, scaled down to fit within `maxSide` when positive.
  [[nodiscard]] Handle loadFile(const std::filesystem::path &file, Filter filter, int maxSide = 0);

  /// On the render thread, once a frame: makes textures of decoded images, oldest first, until
  /// `bytes` of pixels have gone up (always at least one), and hands over the failures.
  ImageUploads upload(SDL_GPUDevice &device, std::size_t bytes);

  /// Blocks until `handle`'s decode has finished, taking it ahead of the queue; for a still frame.
  void wait(const Handle &handle);
  /// Blocks until nothing is queued or decoding.
  void waitAll();
  /// Whether nothing is queued, decoding or decoded and not yet uploaded.
  [[nodiscard]] bool idle() const;

private:
  void run(const std::stop_token &stop);
  void finish(std::shared_ptr<Entry> entry, std::optional<image::Bitmap> pixels, std::string error);

  mutable std::mutex mutex_;
  std::condition_variable_any wake_;
  std::condition_variable settled_;
  std::deque<std::weak_ptr<Entry>> queue_;
  std::deque<std::weak_ptr<Entry>> decoded_;
  std::vector<ImageFailure> failures_;
  std::unordered_map<std::string, std::weak_ptr<Entry>> index_;
  std::size_t busy_{0};
  /// Last, so the workers stop before anything they read is destroyed.
  std::vector<std::jthread> workers_;
};

using ImageHandle = ImageLoader::Handle;

} // namespace gpu2d
