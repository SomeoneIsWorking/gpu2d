// window_platform — what the platform's window system is asked for before SDL video starts. One
// source file per platform, chosen in CMakeLists.txt: X11's ARGB visual, or nothing elsewhere.
#pragma once

namespace gpu2d::window_platform {

/// Sets SDL's hints for the window `GpuWindow` opens next; true when its alpha will reach the
/// compositor.
[[nodiscard]] bool prepareWindow();

} // namespace gpu2d::window_platform
