#include "gpu2d/image_loader.hpp"

#include <algorithm>
#include <exception>
#include <utility>

#include "gpu2d/image/decode.hpp"
#include "gpu2d/image/resize.hpp"

namespace gpu2d {
namespace {

/// One key per image and filter, since each filter is its own texture.
std::string indexKey(const std::string &key, Filter filter) {
  return key + '\x1f' + std::to_string(static_cast<int>(filter));
}

} // namespace

ImageLoader::ImageLoader(std::size_t workers) {
  workers_.reserve(std::max<std::size_t>(workers, 1));
  for (std::size_t i = 0; i < std::max<std::size_t>(workers, 1); ++i) {
    workers_.emplace_back([this](const std::stop_token &stop) {
      run(stop);
    });
  }
}

ImageLoader::~ImageLoader() {
  for (std::jthread &worker : workers_) {
    worker.request_stop();
  }
  wake_.notify_all();
}

ImageLoader::Handle ImageLoader::load(ImageRequest request) {
  std::string key = indexKey(request.key, request.filter);
  {
    const std::lock_guard lock{mutex_};
    if (const auto found = index_.find(key); found != index_.end()) {
      if (std::shared_ptr<Entry> shared = found->second.lock()) {
        return Handle{std::move(shared)};
      }
    }
    auto entry = std::make_shared<Entry>();
    entry->key = std::move(request.key);
    entry->decode = std::move(request.decode);
    entry->filter = request.filter;
    index_.insert_or_assign(std::move(key), entry);
    queue_.push_back(entry);
    wake_.notify_one();
    return Handle{std::move(entry)};
  }
}

ImageLoader::Handle ImageLoader::loadFile(const std::filesystem::path &file, Filter filter,
                                          int maxSide) {
  return load(
      ImageRequest{.key = file.string() + '\x1f' + std::to_string(maxSide),
                   .decode = [file, maxSide](std::string &error) -> std::optional<image::Bitmap> {
                     std::optional<image::Bitmap> pixels = image::decodeFile(file, error);
                     if (pixels && maxSide > 0) {
                       return image::fittedWithin(std::move(*pixels), maxSide);
                     }
                     return pixels;
                   },
                   .filter = filter});
}

void ImageLoader::run(const std::stop_token &stop) {
  while (true) {
    std::shared_ptr<Entry> entry;
    ImageDecode decode;
    {
      std::unique_lock lock{mutex_};
      if (!wake_.wait(lock, stop, [this] {
            return !queue_.empty();
          })) {
        return;
      }
      entry = queue_.front().lock();
      queue_.pop_front();
      if (entry == nullptr) {
        // Its handles were dropped: cancelled.
        settled_.notify_all();
        continue;
      }
      decode = std::move(entry->decode);
      ++busy_;
    }
    std::string error;
    std::optional<image::Bitmap> pixels;
    try {
      pixels = decode(error);
    } catch (const std::exception &thrown) {
      error = thrown.what();
    }
    finish(std::move(entry), std::move(pixels), std::move(error));
  }
}

void ImageLoader::finish(std::shared_ptr<Entry> entry, std::optional<image::Bitmap> pixels,
                         std::string error) {
  const std::lock_guard lock{mutex_};
  --busy_;
  entry->decoded = true;
  if (pixels && !pixels->empty()) {
    entry->pixels = std::move(*pixels);
    decoded_.push_back(entry);
  } else {
    entry->error = error.empty() ? std::string{"the image has no pixels"} : std::move(error);
    failures_.push_back(ImageFailure{entry->key, entry->error});
    entry->state.store(ImageState::Failed, std::memory_order_release);
  }
  // Dropped under the lock: an entry whose handles are gone has no texture yet to release here.
  entry.reset();
  settled_.notify_all();
}

ImageUploads ImageLoader::upload(SDL_GPUDevice &device, std::size_t bytes) {
  ImageUploads uploads;
  std::vector<std::shared_ptr<Entry>> landing;
  {
    const std::lock_guard lock{mutex_};
    uploads.failures = std::exchange(failures_, {});
    std::size_t spent = 0;
    while (!decoded_.empty()) {
      std::shared_ptr<Entry> entry = decoded_.front().lock();
      if (entry == nullptr) {
        decoded_.pop_front();
        continue;
      }
      const std::size_t size = entry->pixels.rgba.size();
      if (!landing.empty() && spent + size > bytes) {
        break;
      }
      decoded_.pop_front();
      spent += size;
      landing.push_back(std::move(entry));
    }
    std::erase_if(index_, [](const auto &pair) {
      return pair.second.expired();
    });
  }
  for (const std::shared_ptr<Entry> &entry : landing) {
    entry->texture.emplace(device, entry->pixels.rgba, entry->pixels.width, entry->pixels.height,
                           entry->filter);
    entry->pixels = {};
    entry->state.store(ImageState::Ready, std::memory_order_release);
  }
  uploads.textures = landing.size();
  return uploads;
}

void ImageLoader::wait(const Handle &handle) {
  const std::shared_ptr<Entry> &entry = handle.entry_;
  if (entry == nullptr) {
    return;
  }
  std::unique_lock lock{mutex_};
  const auto queued = std::ranges::find_if(queue_, [&entry](const std::weak_ptr<Entry> &item) {
    return item.lock() == entry;
  });
  if (queued != queue_.end()) {
    std::rotate(queue_.begin(), queued, std::next(queued));
  }
  settled_.wait(lock, [&entry] {
    return entry->decoded;
  });
}

void ImageLoader::waitAll() {
  std::unique_lock lock{mutex_};
  settled_.wait(lock, [this] {
    return queue_.empty() && busy_ == 0;
  });
}

bool ImageLoader::idle() const {
  const std::lock_guard lock{mutex_};
  return queue_.empty() && busy_ == 0 &&
         std::ranges::all_of(decoded_, [](const std::weak_ptr<Entry> &entry) {
           return entry.expired();
         });
}

} // namespace gpu2d
