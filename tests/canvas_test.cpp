// The canvas through the shipping pipelines on a real device: each primitive drawn offscreen and
// read back, checked pixel by pixel. Values read back are premultiplied.
#include "gpu2d/canvas.hpp"

#include <array>
#include <cstdint>
#include <cstdio>

#include <X11/Xlib.h>

#include "gpu_fixture.hpp"
#include "gpu2d/texture.hpp"

namespace {

using gpu2d::BlendMode;
using gpu2d::Canvas;
using gpu2d::Colour;
using gpu2d::Point;
using gpu2d::Rect;
using gpu2d::Transform;
using gpu2d::test::expect;
using gpu2d::test::expectPixel;
using gpu2d::test::GpuFixture;
using gpu2d::test::Pixel;

constexpr Colour red{255, 0, 0, 255};
constexpr Pixel clear{0, 0, 0, 0};
constexpr Pixel opaqueRed{255, 0, 0, 255};

void roundedRect(GpuFixture &gpu) {
  const auto frame = gpu.draw([](Canvas &canvas) {
    canvas.fillRoundRect(Rect{10.5f, 10.0f, 40.0f, 30.0f}, 10.0f, red);
  });
  expectPixel(frame, 30, 25, opaqueRed, "a rounded rectangle's inside is filled");
  expectPixel(frame, 30, 45, clear, "below it is untouched");
  expectPixel(frame, 11, 11, clear, "its cut corner is untouched");
  // Pixel 10 spans 10 to 11 and the edge is at 10.5: half covered.
  expectPixel(frame, 10, 25, Pixel{128, 0, 0, 128}, "its edge is antialiased", 3);
}

void strokedRect(GpuFixture &gpu) {
  const auto frame = gpu.draw([](Canvas &canvas) {
    canvas.strokeRoundRect(Rect{8.0f, 8.0f, 48.0f, 48.0f}, 8.0f, 4.0f, red);
  });
  expectPixel(frame, 32, 9, opaqueRed, "the stroke lies inside the edge");
  expectPixel(frame, 32, 32, clear, "the middle stays empty");
  expectPixel(frame, 32, 13, clear, "the stroke is four pixels wide");
}

void circleAndRingGap(GpuFixture &gpu) {
  const auto frame = gpu.draw([](Canvas &canvas) {
    // Clockwise on screen from +x: right, down, left, up; the up-right quarter is left open.
    canvas.ring(gpu2d::Ring{.centre = Point{32.0f, 32.0f},
                            .inner = 10.0f,
                            .outer = 16.0f,
                            .startDegrees = 0.0f,
                            .endDegrees = 270.0f},
                red);
    canvas.fillCircle(Point{32.0f, 32.0f}, 4.0f, red);
  });
  expectPixel(frame, 22, 41, opaqueRed, "the ring covers the lower left");
  expectPixel(frame, 45, 32, opaqueRed, "the ring starts at +x");
  expectPixel(frame, 41, 22, clear, "the ring's gap is in the upper right");
  expectPixel(frame, 32, 26, clear, "the ring's hole is empty");
  expectPixel(frame, 32, 32, opaqueRed, "the circle fills the centre");
}

void lineAndTriangle(GpuFixture &gpu) {
  const auto frame = gpu.draw([](Canvas &canvas) {
    canvas.line(Point{8.0f, 10.0f}, Point{56.0f, 10.0f}, 4.0f, red);
    canvas.triangle(Point{0.0f, 64.0f}, Point{64.0f, 64.0f}, Point{0.0f, 20.0f}, red);
  });
  expectPixel(frame, 32, 10, opaqueRed, "the line is drawn");
  expectPixel(frame, 32, 14, clear, "the line is four pixels thick");
  expectPixel(frame, 4, 60, opaqueRed, "the triangle covers its corner");
  expectPixel(frame, 60, 24, clear, "the triangle stops at its hypotenuse");
}

void scissor(GpuFixture &gpu) {
  const auto frame = gpu.draw([](Canvas &canvas) {
    canvas.pushClip(Rect{0.0f, 0.0f, 32.0f, 64.0f});
    canvas.pushClip(Rect{16.0f, 0.0f, 48.0f, 64.0f});
    canvas.fillRect(Rect{0.0f, 0.0f, 64.0f, 32.0f}, red);
    canvas.popClip();
    canvas.fillRect(Rect{0.0f, 32.0f, 64.0f, 32.0f}, red);
    canvas.popClip();
  });
  expectPixel(frame, 20, 10, opaqueRed, "inside both clips is drawn");
  expectPixel(frame, 10, 10, clear, "the inner clip cuts the left");
  expectPixel(frame, 40, 10, clear, "the outer clip still cuts the right");
  expectPixel(frame, 10, 40, opaqueRed, "popping restores the outer clip");
  expectPixel(frame, 40, 40, clear, "the outer clip holds after the pop");
}

void blend(GpuFixture &gpu) {
  const auto frame = gpu.draw(
      [](Canvas &canvas) {
        canvas.fillRect(Rect{0.0f, 0.0f, 32.0f, 64.0f}, Colour{255, 0, 0, 128});
        canvas.setBlend(BlendMode::Replace);
        canvas.fillRect(Rect{32.0f, 0.0f, 32.0f, 64.0f}, Colour{255, 0, 0, 128});
      },
      Colour{0, 0, 255, 255});
  expectPixel(frame, 16, 32, Pixel{128, 0, 127, 255}, "over blends with what is below");
  expectPixel(frame, 48, 32, Pixel{128, 0, 0, 128}, "replace writes the source alone");
}

void transform(GpuFixture &gpu) {
  const auto frame = gpu.draw([](Canvas &canvas) {
    canvas.pushTransform(Transform::translate(Point{20.0f, 20.0f}));
    canvas.pushTransform(Transform::scaleAbout(Point{0.0f, 0.0f}, 2.0f));
    canvas.fillRect(Rect{0.0f, 0.0f, 10.0f, 10.0f}, red);
    canvas.popTransform();
    canvas.popTransform();
    canvas.fillRect(Rect{0.0f, 0.0f, 4.0f, 4.0f}, red);
  });
  expectPixel(frame, 21, 21, opaqueRed, "the shape moves by the outer translation");
  expectPixel(frame, 38, 38, opaqueRed, "and grows by the inner scale");
  expectPixel(frame, 41, 41, clear, "and stops at twice its size");
  expectPixel(frame, 18, 30, clear, "nothing is left at the untransformed place");
  expectPixel(frame, 2, 2, opaqueRed, "popping restores the identity");
}

void textureTint(GpuFixture &gpu) {
  // Top left red, top right green, bottom left blue, bottom right half-transparent white.
  constexpr std::array<std::uint8_t, 16> checker{255, 0, 0,   255, 0,   255, 0,   255,
                                                 0,   0, 255, 255, 255, 255, 255, 128};
  const gpu2d::Texture texture{gpu.device(), checker, 2, 2, gpu2d::Filter::Nearest};
  const auto frame = gpu.draw([&texture](Canvas &canvas) {
    canvas.image(texture.view(), Rect{0.0f, 0.0f, 2.0f, 2.0f}, Rect{0.0f, 0.0f, 64.0f, 64.0f},
                 Colour{255, 255, 255, 255});
    canvas.image(texture.view(), Rect{0.0f, 0.0f, 1.0f, 1.0f}, Rect{0.0f, 0.0f, 16.0f, 16.0f},
                 Colour{0, 255, 255, 128});
  });
  expectPixel(frame, 40, 10, Pixel{0, 255, 0, 255}, "the source's top right lands top right");
  expectPixel(frame, 10, 40, Pixel{0, 0, 255, 255}, "its bottom left lands bottom left");
  expectPixel(frame, 50, 50, Pixel{128, 128, 128, 128}, "straight alpha is premultiplied");
  // Red tinted cyan is black; at half alpha over red it halves the red.
  expectPixel(frame, 8, 8, Pixel{127, 0, 0, 255}, "the tint multiplies the texture");
}

void renderTargetReadBack(GpuFixture &gpu) {
  const gpu2d::RenderTarget other{gpu.device(), 8, 4};
  gpu.canvas().begin(other.surface(), Colour{10, 20, 30, 255});
  const auto frame = gpu.submit(other);
  expect(frame.width == 8 && frame.height == 4, "the read back has the target's size");
  expectPixel(frame, 7, 3, Pixel{10, 20, 30, 255}, "the read back has the cleared colour", 0);
}

void blurSpreads(GpuFixture &gpu) {
  const gpu2d::RenderTarget scene{gpu.device(), GpuFixture::size, GpuFixture::size};
  gpu.canvas().begin(scene.surface(), Colour{0, 0, 0, 255});
  gpu.canvas().fillRect(Rect{30.0f, 0.0f, 4.0f, 64.0f}, Colour{255, 255, 255, 255});
  const auto frame = gpu.draw([&scene](Canvas &canvas) {
    canvas.image(scene.view(), Rect{0.0f, 0.0f, 64.0f, 64.0f}, Rect{0.0f, 0.0f, 64.0f, 64.0f},
                 Colour{255, 255, 255, 255});
    canvas.blurBehind(gpu2d::Backdrop{.scene = &scene,
                                      .body = Rect{8.0f, 16.0f, 48.0f, 32.0f},
                                      .radius = 16.0f,
                                      .sigma = 4.0f,
                                      .opacity = 1.0f});
  });
  const Pixel beside = gpu2d::test::at(frame, 26, 32);
  expect(beside.r > 20 && beside.r < 200, "the blur spreads the stripe sideways");
  expect(beside.a == 255, "the blur keeps an opaque scene opaque");
  expect(gpu2d::test::at(frame, 31, 32).r < 250, "the blur softens the stripe itself");
  expectPixel(frame, 26, 8, Pixel{0, 0, 0, 255}, "above the body the scene is sharp", 0);
  expectPixel(frame, 31, 8, Pixel{255, 255, 255, 255}, "the stripe is sharp outside the body", 0);
  expectPixel(frame, 9, 17, Pixel{0, 0, 0, 255}, "the mask's corner shows the scene", 0);
}

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
  roundedRect(gpu);
  strokedRect(gpu);
  circleAndRingGap(gpu);
  lineAndTriangle(gpu);
  scissor(gpu);
  blend(gpu);
  transform(gpu);
  textureTint(gpu);
  renderTargetReadBack(gpu);
  blurSpreads(gpu);
  std::printf("canvas: all checks passed\n");
  return 0;
}
