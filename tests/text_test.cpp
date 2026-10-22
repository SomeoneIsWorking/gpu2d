// Text through SDL3_ttf's GPU engine and the canvas: a face measures, draws its coverage where
// the line box says, and letter spacing widens a label by one spacing per character.
#include "gpu2d/text.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>

#include "gpu_fixture.hpp"

namespace {

using gpu2d::Canvas;
using gpu2d::Colour;
using gpu2d::Point;
using gpu2d::Text;
using gpu2d::TextStyle;
using gpu2d::test::at;
using gpu2d::test::expect;
using gpu2d::test::GpuFixture;
using gpu2d::test::near;

/// Pixels from left to right (exclusive) and top to bottom (exclusive).
struct Area {
  int left{};
  int top{};
  int right{};
  int bottom{};
};

int coverage(const gpu2d::image::Bitmap &frame, const Area &area) {
  int covered = 0;
  for (int y = area.top; y < area.bottom; ++y) {
    for (int x = area.left; x < area.right; ++x) {
      covered += at(frame, x, y).a > 64 ? 1 : 0;
    }
  }
  return covered;
}

void measures(Text &text) {
  const TextStyle plain{.size = 24.0f, .tracking = 0.0f};
  const float width = text.measure("Hi", plain);
  expect(width > 10.0f && width < 40.0f, "a short label has a plausible width");
  expect(text.measure("", plain) == 0.0f, "an empty label has no width");
  const TextStyle spaced{.size = 24.0f, .tracking = 0.1f};
  near(text.measure("Hi", spaced), width + 2.0f * 2.4f, "spacing adds per character", 0.01);
  const auto metrics = text.metrics(24.0f);
  expect(metrics.ascent > 0.0f && metrics.descent > 0.0f, "the face has an ascent and descent");
}

void draws(GpuFixture &gpu, Text &text) {
  const TextStyle style{.size = 24.0f, .tracking = 0.0f};
  const float width = text.measure("Hi", style);
  const auto box = text.metrics(24.0f).lineBox();
  const Point origin{8.0f, 10.0f};
  const auto frame = gpu.draw([&](Canvas &canvas) {
    expect(text.draw(canvas, "Hi", origin, style, Colour{255, 255, 255, 255}), "the label shapes");
  });
  text.retire();
  const int left = 8;
  const int top = 10;
  const int right = left + static_cast<int>(width) + 1;
  const int bottom = top + static_cast<int>(box) + 1;
  expect(coverage(frame, Area{left, top, right, bottom}) > 40, "the label covers its line box");
  expect(coverage(frame, Area{right + 1, 0, 64, 64}) == 0, "nothing is drawn right of its width");
  expect(coverage(frame, Area{0, 0, 64, top}) == 0, "nothing is drawn above its line box");
  expect(coverage(frame, Area{0, 0, left, 64}) == 0, "nothing is drawn left of its origin");
  const auto ink = at(frame, left + 2, top + static_cast<int>(box * 0.5f));
  expect(ink.r == ink.a, "white text reads back premultiplied white");
}

void spacingMovesGlyphs(GpuFixture &gpu, Text &text) {
  const TextStyle spaced{.size = 24.0f, .tracking = 0.5f};
  const float width = text.measure("II", spaced);
  const auto frame = gpu.draw([&](Canvas &canvas) {
    expect(text.draw(canvas, "II", Point{0.0f, 0.0f}, spaced, Colour{255, 255, 255, 255}),
           "the spaced label shapes");
  });
  text.retire();
  // Half a spacing (6 px) before the first glyph.
  expect(coverage(frame, Area{0, 0, 5, 64}) == 0, "spacing opens before the first character");
  expect(coverage(frame, Area{static_cast<int>(width * 0.5f) - 4, 0,
                              static_cast<int>(width * 0.5f) + 4, 64}) == 0,
         "spacing opens between the characters");
}

void inkMatchesWhatIsDrawn(GpuFixture &gpu, Text &text) {
  const TextStyle style{.size = 24.0f, .tracking = 0.0f};
  const gpu2d::GlyphInk ink = text.ink(U'o', 24.0f);
  expect(ink.right > ink.left && ink.top > ink.bottom && ink.bottom >= -1.0f,
         "a lowercase o inks a box sitting on the baseline");
  const gpu2d::GlyphInk dash = text.ink(U'-', 24.0f);
  expect(dash.bottom > ink.bottom && dash.top < text.ink(U'H', 24.0f).top,
         "a dash inks between the baseline and the capital height");
  const float ascent = text.metrics(24.0f).ascent;
  const auto frame = gpu.draw([&](Canvas &canvas) {
    expect(text.draw(canvas, "o", Point{10.0f, 10.0f}, style, Colour{255, 255, 255, 255}),
           "the glyph shapes");
  });
  text.retire();
  int left = 64;
  int right = 0;
  int top = 64;
  int bottom = 0;
  for (int y = 0; y < 64; ++y) {
    for (int x = 0; x < 64; ++x) {
      if (at(frame, x, y).a > 64) {
        left = std::min(left, x);
        right = std::max(right, x + 1);
        top = std::min(top, y);
        bottom = std::max(bottom, y + 1);
      }
    }
  }
  const float baseline = 10.0f + ascent;
  near(static_cast<float>(left), 10.0f + ink.left, "the ink starts where its box says", 1.5);
  near(static_cast<float>(right), 10.0f + ink.right, "and ends there", 1.5);
  near(static_cast<float>(top), baseline - ink.top, "its top is the box's", 1.5);
  near(static_cast<float>(bottom), baseline - ink.bottom, "and its bottom", 1.5);
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: render_text_test FACE.ttf\n");
    return 2;
  }
  GpuFixture gpu;
  Text text{gpu.device(), std::filesystem::path{argv[1]},
            gpu2d::FaceOptions{.hinted = false, .kerned = false}};
  measures(text);
  draws(gpu, text);
  spacingMovesGlyphs(gpu, text);
  inkMatchesWhatIsDrawn(gpu, text);
  std::printf("text: all checks passed\n");
  return 0;
}
