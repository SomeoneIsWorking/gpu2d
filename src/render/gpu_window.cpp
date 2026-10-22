#include "gpu2d/gpu_window.hpp"

#include <array>
#include <string>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_properties.h>

#include "gpu2d/gpu_error.hpp"
#include "window_platform.hpp"

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

} // namespace

GpuWindow::GpuWindow(const WindowOptions &options) {
  try {
    translucent_ = window_platform::prepareWindow();
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
