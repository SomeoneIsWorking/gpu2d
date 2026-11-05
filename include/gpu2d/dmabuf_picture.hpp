// dmabuf_picture — a 4:2:0 picture with interleaved chroma (NV12's layout) that lies in DMA-BUFs,
// such as a hardware video decoder's, sampled where it lies and drawn into an RGBA target on the
// GPU, with no copy through the CPU. Each picture's planes are wrapped for one draw and its owner
// kept until the GPU has read them. Only a device `GpuWindow` opened with `importsDmabuf` can.
#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>

#include <SDL3/SDL_gpu.h>

#include "gpu2d/gpu_handle.hpp"
#include "gpu2d/image/bitmap.hpp"
#include "gpu2d/pipeline.hpp"
#include "gpu2d/render_target.hpp"
#include "gpu2d/texture.hpp"

namespace gpu2d {

/// One plane of a picture in a DMA-BUF.
struct DmabufPlane {
  /// Borrowed: the picture's owner keeps it open.
  int fd{-1};
  std::uint64_t offset{};
  /// Bytes from one row to the next.
  std::uint64_t pitch{};
  /// The DRM format modifier: how the rows are tiled.
  std::uint64_t modifier{};
};

/// A `width` x `height` picture (both even): luma one byte a sample, then chroma at half the size
/// each way, two bytes a sample (Cb, then Cr).
struct DmabufYcbcr {
  DmabufPlane luma;
  DmabufPlane chroma;
  int width{};
  int height{};
  /// Keeps the buffers open and their contents unchanged until the GPU has read them.
  std::shared_ptr<const void> owner;
};

/// RGB from YCbCr samples in [0, 1]: each of red, green and blue is its row's dot product with
/// (Y, Cb, Cr, 1).
struct YcbcrToRgb {
  float red[4]{};
  float green[4]{};
  float blue[4]{};
};

class DmabufPicture {
public:
  /// Throws GpuError when the pipeline cannot be made.
  explicit DmabufPicture(SDL_GPUDevice &device);
  /// Waits for the GPU to finish with the pictures it still reads.
  ~DmabufPicture();
  DmabufPicture(const DmabufPicture &) = delete;
  DmabufPicture &operator=(const DmabufPicture &) = delete;
  DmabufPicture(DmabufPicture &&) = delete;
  DmabufPicture &operator=(DmabufPicture &&) = delete;

  /// Draws `picture` through `colour` into the target, which then holds it; returns once the
  /// draw is submitted, so later submissions on the device see it. Throws GpuError when the device
  /// cannot wrap its planes (no import, or a format and modifier its driver cannot sample).
  void show(DmabufYcbcr picture, const YcbcrToRgb &colour);

  /// The picture shown last, opaque; nothing before the first.
  [[nodiscard]] std::optional<TextureView> view(Filter filter = Filter::Linear) const noexcept;
  /// Its pixels (RGBA, rows top first); empty before the first. Throws GpuError.
  [[nodiscard]] image::Bitmap read() const;

private:
  /// A shown picture's planes, kept until the fence of the draw that read them signals.
  struct Reading {
    FenceHandle fence;
    std::shared_ptr<const void> owner;
  };
  /// Lets go of the pictures the GPU has finished reading.
  void retire();

  SDL_GPUDevice *device_;
  PipelineSet pipeline_;
  SamplerHandle sampler_;
  std::optional<RenderTarget> target_;
  std::deque<Reading> reading_;
};

} // namespace gpu2d
