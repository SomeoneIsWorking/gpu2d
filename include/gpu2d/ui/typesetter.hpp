// typesetter — what the app lends the toolkit to draw words and button prompts: its face and its
// controller glyphs. Sizes are ems in pixels.
#pragma once

#include <string_view>

#include "gpu2d/canvas.hpp"
#include "gpu2d/geometry.hpp"
#include "gpu2d/ui/input.hpp"

namespace gpu2d::ui {

class Typesetter {
public:
  Typesetter() = default;
  virtual ~Typesetter() = default;
  Typesetter(const Typesetter &) = delete;
  Typesetter &operator=(const Typesetter &) = delete;
  Typesetter(Typesetter &&) = delete;
  Typesetter &operator=(Typesetter &&) = delete;

  /// The width `text` advances at `size`.
  [[nodiscard]] virtual float measure(std::string_view text, float size) = 0;
  /// The height of a line at `size`, ascent to descent.
  [[nodiscard]] virtual float lineHeight(float size) = 0;
  /// Draws `text` with the top of its line at `topLeft.y`.
  virtual void draw(Canvas &canvas, std::string_view text, Point topLeft, float size,
                    Colour colour) = 0;

  /// Draws `text` from `start.x` with its line centred on `start.y`.
  void drawCentred(Canvas &canvas, std::string_view text, Point start, float size, Colour colour);
  /// The size whose line is `height` tall.
  [[nodiscard]] float sizeForLineHeight(float height);
};

class Prompts {
public:
  Prompts() = default;
  virtual ~Prompts() = default;
  Prompts(const Prompts &) = delete;
  Prompts &operator=(const Prompts &) = delete;
  Prompts(Prompts &&) = delete;
  Prompts &operator=(Prompts &&) = delete;

  /// How wide `action`'s glyph is when it is `height` tall.
  [[nodiscard]] virtual float width(Action action, float height) = 0;
  /// Draws `action`'s glyph in `box`.
  virtual void draw(Canvas &canvas, Action action, const Rect &box, Colour ink) = 0;
  /// Whether the device in use has a control for `action` now; `typing` while a text field holds
  /// the focus, which may take keys for typing. A hint naming an unbound action is not shown.
  [[nodiscard]] virtual bool bound(Action /*action*/, bool /*typing*/) {
    return true;
  }
};

} // namespace gpu2d::ui
