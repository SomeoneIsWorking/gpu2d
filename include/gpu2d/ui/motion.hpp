// motion — the toolkit's one clock and what moves on it: a tween over a fixed length and a follow
// that eases towards a target that may keep moving (scrolling, the focus ring). Times are seconds
// on the app's clock, which it hands the navigator each frame.
#pragma once

#include "gpu2d/geometry.hpp"

namespace gpu2d::ui {

/// When a move starts and how long it takes.
struct Timing {
  /// The clock now, in seconds.
  double now{};
  float milliseconds{};
};

/// Slow at the end.
[[nodiscard]] float easeOut(float t) noexcept;
/// Slow at both ends.
[[nodiscard]] float easeInOut(float t) noexcept;

/// A value moving from one place to another over a fixed time, eased out.
class Tween {
public:
  /// Stands at `value` at once.
  void snap(float value) noexcept;
  /// Moves from where it is at `timing.now` to `target` over `timing.milliseconds`.
  void to(float target, Timing timing) noexcept;
  [[nodiscard]] float value(double now) const noexcept;
  [[nodiscard]] float target() const noexcept {
    return to_;
  }
  [[nodiscard]] bool moving(double now) const noexcept;

private:
  float from_{};
  float to_{};
  double start_{};
  float length_{};
};

/// A value easing towards a target exponentially: a fixed share of the way each `milliseconds`,
/// so a target that moves mid-way is followed without a jump.
class Follow {
public:
  void snap(float value) noexcept;
  void aim(float target) noexcept {
    target_ = target;
  }
  /// Moves towards the target for the time since the last step, closing most of the gap in
  /// `timing.milliseconds`; returns the value.
  float step(Timing timing) noexcept;
  [[nodiscard]] float value() const noexcept {
    return value_;
  }
  [[nodiscard]] float target() const noexcept {
    return target_;
  }
  [[nodiscard]] bool settled() const noexcept;

private:
  float value_{};
  float target_{};
  double last_{-1.0};
};

/// A rectangle following its target, each edge a `Follow`.
class RectFollow {
public:
  void snap(const Rect &rect) noexcept;
  void aim(const Rect &rect) noexcept;
  Rect step(Timing timing) noexcept;
  [[nodiscard]] Rect value() const noexcept;
  [[nodiscard]] bool settled() const noexcept;
  [[nodiscard]] bool placed() const noexcept {
    return placed_;
  }

private:
  Follow x_;
  Follow y_;
  Follow width_;
  Follow height_;
  bool placed_{false};
};

} // namespace gpu2d::ui
