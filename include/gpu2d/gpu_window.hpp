// gpu_window — an SDL window and the SDL GPU device (Vulkan) that draws into it, made and claimed
// together. Always X11 (overlay properties such as Gamescope's are X11 window properties), on a
// 32-bit visual where the server has one, so the window's premultiplied alpha is what a compositor
// blends with what lies below. Hidden is an unmapped window that presents without vsync.
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_video.h>

#include "gpu2d/surface.hpp"

namespace gpu2d {

struct WindowOptions {
  std::string title;
  int width{};
  int height{};
  bool hidden{false};
};

/// A size in pixels.
struct PixelSize {
  int width{};
  int height{};
};

class GpuWindow {
public:
  /// Opens the window and device; throws GpuError when either cannot be had.
  explicit GpuWindow(const WindowOptions &options);
  ~GpuWindow();
  GpuWindow(const GpuWindow &) = delete;
  GpuWindow &operator=(const GpuWindow &) = delete;
  GpuWindow(GpuWindow &&) = delete;
  GpuWindow &operator=(GpuWindow &&) = delete;

  [[nodiscard]] SDL_GPUDevice &device() const noexcept {
    return *device_;
  }
  [[nodiscard]] SDL_Window *window() const noexcept {
    return window_;
  }
  [[nodiscard]] PixelSize pixelSize() const;
  /// The GPU's name as the driver reports it.
  [[nodiscard]] std::string deviceName() const;
  /// SDL's GPU driver, "vulkan".
  [[nodiscard]] std::string driver() const;
  /// Whether the window has a 32-bit visual, so its alpha reaches the compositor; false when
  /// the X server has none and the window is opaque.
  [[nodiscard]] bool translucent() const noexcept {
    return translucent_;
  }
  /// The X11 window id, for window properties such as Gamescope's overlay.
  [[nodiscard]] std::uint64_t x11Window() const;
  /// The present mode in use: immediate (or mailbox) when hidden, vsync when shown.
  [[nodiscard]] SDL_GPUPresentMode presentMode() const noexcept {
    return presentMode_;
  }

  /// The swapchain image to draw this frame into, acquired on `commands`; nothing when there is
  /// none to draw (a minimised window). Throws GpuError when acquiring fails.
  [[nodiscard]] std::optional<Surface> acquire(SDL_GPUCommandBuffer *commands);

private:
  void release() noexcept;

  bool videoUp_{false};
  SDL_Window *window_{};
  SDL_GPUDevice *device_{};
  bool claimed_{false};
  bool translucent_{false};
  SDL_GPUPresentMode presentMode_{SDL_GPU_PRESENTMODE_VSYNC};
};

} // namespace gpu2d
