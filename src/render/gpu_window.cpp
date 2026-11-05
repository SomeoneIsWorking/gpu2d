#include "gpu2d/gpu_window.hpp"

#include <array>
#include <string>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_properties.h>

#include "gpu2d/gpu_error.hpp"
#include "gpu2d/pipeline.hpp"
#include "window_platform.hpp"

namespace gpu2d {
namespace {

// SDL GPU's backend on Apple systems is Metal; everywhere else gpu2d draws through Vulkan.
#ifdef __APPLE__
constexpr const char *gpuDriver = "metal";
#else
constexpr const char *gpuDriver = "vulkan";
#endif

#if defined(__linux__) && defined(SDL_PROP_GPU_TEXTURE_CREATE_VULKAN_DMABUF_FD_NUMBER)
#define GPU2D_DMABUF_IMPORT
/// VK_API_VERSION_1_2: external memory, dedicated allocations and image format lists are core.
constexpr Uint32 vulkan12 = (1U << 22U) | (2U << 12U);
/// What SDL needs opted into to wrap a DMA-BUF plane as a texture.
constexpr std::array<const char *, 4> dmabufExtensions{
    "VK_KHR_external_memory_fd", "VK_EXT_external_memory_dma_buf",
    "VK_EXT_image_drm_format_modifier", "VK_EXT_queue_family_foreign"};
#endif

/// A device of gpuDriver taking gpu2d's shader formats; with `dmabuf`, one that can also import
/// DMA-BUFs, or none where the driver cannot.
SDL_GPUDevice *openDevice(bool dmabuf) {
  const SDL_PropertiesID props = SDL_CreateProperties();
  const SDL_GPUShaderFormat formats = builtShaderFormats();
  SDL_SetStringProperty(props, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, gpuDriver);
  SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,
                         (formats & SDL_GPU_SHADERFORMAT_SPIRV) != 0);
  SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_MSL_BOOLEAN,
                         (formats & SDL_GPU_SHADERFORMAT_MSL) != 0);
#ifdef GPU2D_DMABUF_IMPORT
  std::array<const char *, dmabufExtensions.size()> names = dmabufExtensions;
  SDL_GPUVulkanOptions options{};
  options.vulkan_api_version = vulkan12;
  options.device_extension_count = static_cast<Uint32>(names.size());
  options.device_extension_names = names.data();
  if (dmabuf) {
    SDL_SetPointerProperty(props, SDL_PROP_GPU_DEVICE_CREATE_VULKAN_OPTIONS_POINTER, &options);
  }
#else
  if (dmabuf) {
    SDL_DestroyProperties(props);
    return nullptr;
  }
#endif
  SDL_GPUDevice *device = SDL_CreateGPUDeviceWithProperties(props);
  SDL_DestroyProperties(props);
  return device;
}

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
    device_ = openDevice(true);
    importsDmabuf_ = device_ != nullptr;
    if (device_ == nullptr) {
      device_ = openDevice(false);
    }
    if (device_ == nullptr) {
      throw GpuError{std::string{"cannot open a "} + gpuDriver + " GPU device"};
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
