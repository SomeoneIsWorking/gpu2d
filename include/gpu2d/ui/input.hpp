// input — what the toolkit is driven with: navigation actions and pointer events. The app maps its
// own pads, keys and mouse onto these, so nothing here knows a device.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "gpu2d/geometry.hpp"

namespace gpu2d::ui {

enum class Action : std::uint8_t {
  Up,
  Down,
  Left,
  Right,
  /// Presses what is focused.
  Accept,
  /// Closes the screen on top, or steps back within it.
  Back,
  /// A widget's second action.
  Secondary,
  /// A widget's third action.
  Tertiary,
  PreviousTab,
  NextTab,
  /// The app's menu button.
  Menu,
};

enum class PointerKind : std::uint8_t {
  Move,
  /// The primary button went down.
  Press,
  /// Wheel notches in `wheel`, positive away from the player.
  Wheel,
};

struct PointerEvent {
  PointerKind kind{PointerKind::Move};
  Point at;
  float wheel{};
};

/// A prompt the hint bar shows: the actions' glyphs side by side, then the label.
struct Hint {
  std::vector<Action> keys;
  std::string label;
};

} // namespace gpu2d::ui
