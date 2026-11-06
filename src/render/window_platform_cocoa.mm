#include "window_platform.hpp"

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>

namespace gpu2d::window_platform {

bool prepareWindow() {
  return false;
}

// SDL GPU refuses SDL_WINDOW_TRANSPARENT, so the claimed window and its Metal layer are changed.
bool finishWindow(SDL_Window *window) {
  auto *cocoa = (__bridge NSWindow *)SDL_GetPointerProperty(
      SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
  if (cocoa == nil) {
    return false;
  }
  cocoa.opaque = NO;
  cocoa.backgroundColor = NSColor.clearColor;
  bool layerFound = false;
  for (NSView *view in cocoa.contentView.subviews) {
    if ([view.layer isKindOfClass:[CAMetalLayer class]]) {
      view.layer.opaque = NO;
      layerFound = true;
    }
  }
  return layerFound;
}

} // namespace gpu2d::window_platform
