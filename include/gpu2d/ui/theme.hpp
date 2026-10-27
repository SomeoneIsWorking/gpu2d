// theme — the tokens every widget paints with: colours, corner radii, outline and focus ring
// widths, spacing, type sizes and motion lengths. Lengths are in dp; `scale` is the pixels per dp
// the interface size sets. The app supplies every value.
#pragma once

#include "gpu2d/geometry.hpp"

namespace gpu2d::ui {

struct Palette {
  /// Behind a screen.
  Colour ground{};
  /// A pane or card that rows sit on.
  Colour surface{};
  /// A row, button or tab at rest.
  Colour control{};
  /// A focused row, button or tab.
  Colour controlFocused{};
  /// A tab that is chosen but not focused.
  Colour selected{};
  Colour ink{};
  /// Subtitles and values.
  Colour inkMuted{};
  /// Section headings and what cannot be pressed.
  Colour inkFaint{};
  /// A switch that is on, a slider's fill, a primary button.
  Colour accent{};
  /// Text and knobs on `accent`.
  Colour onAccent{};
  /// A switch that is off, a slider's track.
  Colour track{};
  Colour focusRing{};
  /// Over the screens under a dialog.
  Colour scrim{};
  Colour toast{};
  Colour toastError{};
  Colour onToast{};
  /// A button that throws something away.
  Colour danger{};
};

struct Spacing {
  /// Between rows of a list.
  float gap{6.0f};
  /// Inside a row or card, and around a pane.
  float pad{18.0f};
  /// Above a section heading.
  float section{22.0f};
  float rowHeight{68.0f};
  float compactRowHeight{52.0f};
  float sidebarWidth{280.0f};
  /// The widest a column of rows grows.
  float contentWidth{900.0f};
  float hintBarHeight{60.0f};
  float dialogWidth{560.0f};
};

struct Shape {
  float radius{16.0f};
  float smallRadius{10.0f};
  float outline{1.5f};
  float focusRing{3.0f};
  /// Between a widget's edge and its focus ring.
  float focusGap{3.0f};
};

/// Ems in dp.
struct TypeScale {
  float title{30.0f};
  float heading{14.0f};
  float body{19.0f};
  float caption{15.0f};
  float value{17.0f};
  float hint{16.0f};
  float toast{18.0f};
};

/// Lengths in milliseconds.
struct Motion {
  float screen{220.0f};
  float scroll{110.0f};
  float focus{70.0f};
  float toast{200.0f};
};

struct Theme {
  Palette colours;
  Spacing spacing;
  Shape shape;
  TypeScale type;
  Motion motion;
  /// Pixels per dp.
  float scale{1.0f};

  [[nodiscard]] float px(float dp) const noexcept {
    return dp * scale;
  }
};

/// `colour` with its alpha multiplied by `alpha`.
[[nodiscard]] Colour faded(Colour colour, float alpha) noexcept;

/// The colour `t` of the way from `from` to `to`.
[[nodiscard]] Colour mix(Colour from, Colour to, float t) noexcept;

} // namespace gpu2d::ui
