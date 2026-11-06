// window_platform — what the platform's window system is asked for around opening a window. One
// source file per platform, chosen in CMakeLists.txt: X11's ARGB visual, Cocoa's non-opaque window
// and Metal layer, or nothing elsewhere.
#pragma once

struct SDL_Window;

namespace gpu2d::window_platform {

/// Sets SDL's hints for the window `GpuWindow` opens next; true when its alpha will reach the
/// compositor.
[[nodiscard]] bool prepareWindow();

/// Asks `window`, which the GPU device has claimed, for what SDL GPU would refuse beforehand; true
/// when its alpha will reach the compositor.
[[nodiscard]] bool finishWindow(SDL_Window *window);

} // namespace gpu2d::window_platform
