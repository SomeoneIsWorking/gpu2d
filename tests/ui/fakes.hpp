// Stand-ins for the app's face and glyphs: every byte half an em wide, a line 1.25 ems, nothing
// drawn. Layout, focus and the stack need no GPU.
#pragma once

#include <string_view>

#include "gpu2d/ui/navigator.hpp"
#include "gpu2d/ui/typesetter.hpp"

namespace gpu2d::test {

class FakeText final : public ui::Typesetter {
public:
  [[nodiscard]] float measure(std::string_view text, float size) override {
    return static_cast<float>(text.size()) * size * 0.5f;
  }
  [[nodiscard]] float lineHeight(float size) override {
    return size * 1.25f;
  }
  void draw(Canvas & /*canvas*/, std::string_view /*text*/, Point /*topLeft*/, float /*size*/,
            Colour /*colour*/) override {
  }
};

class FakePrompts final : public ui::Prompts {
public:
  [[nodiscard]] float width(ui::Action /*action*/, float height) override {
    return height;
  }
  void draw(Canvas & /*canvas*/, ui::Action /*action*/, const Rect & /*box*/,
            Colour /*ink*/) override {
  }
};

/// A navigator over the fakes at 1 px per dp in a 1280 x 800 viewport, counting its sounds.
struct Rig {
  FakeText text;
  FakePrompts prompts;
  int moves{0};
  int activations{0};
  int backs{0};
  ui::Navigator navigator{text, prompts,
                          ui::Navigator::Sounds{[this] {
                                                  ++moves;
                                                },
                                                [this] {
                                                  ++activations;
                                                },
                                                [this] {
                                                  ++backs;
                                                }}};
  double now{0.0};
  gpu2d::Rect viewport{0.0f, 0.0f, 1280.0f, 800.0f};

  /// Lays the stack out after `seconds` more on the clock.
  void frame(double seconds = 0.0) {
    now += seconds;
    navigator.setTime(now);
    navigator.layout(viewport);
  }
};

} // namespace gpu2d::test
