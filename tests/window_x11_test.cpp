// The hidden window where windows are X11's: a 32-bit visual, never mapped, presenting without
// vsync and with no image to draw.
#include <cstdint>
#include <cstdio>

#include <X11/Xlib.h>

#include "gpu_fixture.hpp"

namespace {

using gpu2d::test::expect;
using gpu2d::test::GpuFixture;

void hiddenWindow(GpuFixture &gpu) {
  const std::uint64_t id = gpu.window().x11Window();
  expect(id != 0, "the window is an X11 window");
  Display *display = XOpenDisplay(nullptr);
  expect(display != nullptr, "the test reaches the X server");
  XWindowAttributes attributes{};
  XGetWindowAttributes(display, static_cast<Window>(id), &attributes);
  XCloseDisplay(display);
  expect(attributes.depth == 32, "the window has an alpha channel for the compositor");
  expect(attributes.map_state == IsUnmapped, "a hidden window is never mapped");
  expect(gpu.window().presentMode() != SDL_GPU_PRESENTMODE_VSYNC,
         "a hidden window presents without vsync");
  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(&gpu.device());
  expect(!gpu.window().acquire(commands), "a hidden window has no image to present");
  expect(SDL_SubmitGPUCommandBuffer(commands), "the empty frame submits");
}

} // namespace

int main() {
  GpuFixture gpu;
  hiddenWindow(gpu);
  std::printf("window_x11: all checks passed\n");
  return 0;
}
