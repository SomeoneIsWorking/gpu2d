#include "gpu2d/gpu_window.hpp"

#include <array>
#include <memory>
#include <string>

#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_properties.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "gpu2d/gpu_error.hpp"

namespace gpu2d {
namespace {

/// Without vsync when hidden: nothing is seen, and a frame should not wait for a display.
SDL_GPUPresentMode choosePresentMode(SDL_GPUDevice *device, SDL_Window *window, bool hidden) {
  if (!hidden) {
    return SDL_GPU_PRESENTMODE_VSYNC;
  }
  for (const SDL_GPUPresentMode mode :
       std::array{SDL_GPU_PRESENTMODE_IMMEDIATE, SDL_GPU_PRESENTMODE_MAILBOX}) {
    if (SDL_WindowSupportsGPUPresentMode(device, window, mode)) {
      return mode;
    }
  }
  return SDL_GPU_PRESENTMODE_VSYNC;
}

struct DisplayClose {
  void operator()(Display *display) const noexcept {
    XCloseDisplay(display);
  }
};

/// A 32-bit TrueColor visual's id, or nothing when the X server has none. SDL GPU refuses
/// SDL_WINDOW_TRANSPARENT, and SDL gives a window without it the default 24-bit visual; on an
/// ARGB visual Mesa's swapchain inherits the window's alpha instead.
std::optional<std::string> argbVisual() {
  const std::unique_ptr<Display, DisplayClose> display{XOpenDisplay(nullptr)};
  if (display == nullptr) {
    return std::nullopt;
  }
  XVisualInfo visual{};
  if (XMatchVisualInfo(display.get(), DefaultScreen(display.get()), 32, TrueColor, &visual) == 0) {
    return std::nullopt;
  }
  return std::to_string(visual.visualid);
}

} // namespace

GpuWindow::GpuWindow(const WindowOptions &options) {
  try {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
    if (const std::optional<std::string> visual = argbVisual()) {
      SDL_SetHint(SDL_HINT_VIDEO_X11_WINDOW_VISUALID, visual->c_str());
      translucent_ = true;
    }
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
      throw GpuError{"cannot start SDL video"};
    }
    videoUp_ = true;
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;
    if (options.hidden) {
      flags |= SDL_WINDOW_HIDDEN;
    }
    window_ = SDL_CreateWindow(options.title.c_str(), options.width, options.height, flags);
    if (window_ == nullptr) {
      throw GpuError{"cannot open a window"};
    }
    device_ = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, "vulkan");
    if (device_ == nullptr) {
      throw GpuError{"cannot open a Vulkan GPU device"};
    }
    if (!SDL_ClaimWindowForGPUDevice(device_, window_)) {
      throw GpuError{"cannot draw into the window"};
    }
    claimed_ = true;
    presentMode_ = choosePresentMode(device_, window_, options.hidden);
    if (!SDL_SetGPUSwapchainParameters(device_, window_, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                       presentMode_)) {
      throw GpuError{"cannot set the present mode"};
    }
  } catch (...) {
    release();
    throw;
  }
}

GpuWindow::~GpuWindow() {
  release();
}

void GpuWindow::release() noexcept {
  if (device_ != nullptr) {
    SDL_WaitForGPUIdle(device_);
    if (claimed_) {
      SDL_ReleaseWindowFromGPUDevice(device_, window_);
      claimed_ = false;
    }
    SDL_DestroyGPUDevice(device_);
    device_ = nullptr;
  }
  if (window_ != nullptr) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }
  if (videoUp_) {
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    videoUp_ = false;
  }
}

PixelSize GpuWindow::pixelSize() const {
  PixelSize size;
  if (!SDL_GetWindowSizeInPixels(window_, &size.width, &size.height)) {
    throw GpuError{"cannot read the window's size"};
  }
  return size;
}

std::string GpuWindow::deviceName() const {
  return SDL_GetStringProperty(SDL_GetGPUDeviceProperties(device_), SDL_PROP_GPU_DEVICE_NAME_STRING,
                               "unnamed GPU");
}

std::string GpuWindow::driver() const {
  return SDL_GetGPUDeviceDriver(device_);
}

std::uint64_t GpuWindow::x11Window() const {
  return static_cast<std::uint64_t>(SDL_GetNumberProperty(SDL_GetWindowProperties(window_),
                                                          SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
}

std::optional<Surface> GpuWindow::acquire(SDL_GPUCommandBuffer *commands) {
  SDL_GPUTexture *texture = nullptr;
  Uint32 width = 0;
  Uint32 height = 0;
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, window_, &texture, &width, &height)) {
    throw GpuError{"cannot acquire the window's image"};
  }
  if (texture == nullptr) {
    return std::nullopt;
  }
  return Surface{texture, static_cast<int>(width), static_cast<int>(height),
                 SDL_GetGPUSwapchainTextureFormat(device_, window_)};
}

} // namespace gpu2d
