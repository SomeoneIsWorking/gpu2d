#include "window_platform.hpp"

namespace gpu2d::window_platform {

// SDL GPU refuses SDL_WINDOW_TRANSPARENT, so outside X11 the window is opaque.
bool prepareWindow() {
  return false;
}

} // namespace gpu2d::window_platform
