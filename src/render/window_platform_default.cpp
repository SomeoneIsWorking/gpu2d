#include "window_platform.hpp"

namespace gpu2d::window_platform {

// SDL GPU refuses SDL_WINDOW_TRANSPARENT; an Android surface's format is its Activity's to set.
bool prepareWindow() {
  return false;
}

bool finishWindow(SDL_Window * /*window*/) {
  return false;
}

} // namespace gpu2d::window_platform
