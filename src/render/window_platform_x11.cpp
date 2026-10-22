#include "window_platform.hpp"

#include <memory>
#include <optional>
#include <string>

#include <SDL3/SDL_hints.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

namespace gpu2d::window_platform {
namespace {

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

bool prepareWindow() {
  // Overlay properties such as Gamescope's are X11 window properties.
  SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
  if (const std::optional<std::string> visual = argbVisual()) {
    SDL_SetHint(SDL_HINT_VIDEO_X11_WINDOW_VISUALID, visual->c_str());
    return true;
  }
  return false;
}

} // namespace gpu2d::window_platform
