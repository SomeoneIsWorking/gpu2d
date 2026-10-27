#include "gpu2d/ui/motion.hpp"

#include <algorithm>
#include <cmath>

namespace gpu2d::ui {
namespace {

/// Closer than this is there.
constexpr float settledWithin = 0.25f;

} // namespace

float easeOut(float t) noexcept {
  const float u = 1.0f - std::clamp(t, 0.0f, 1.0f);
  return 1.0f - u * u * u;
}

float easeInOut(float t) noexcept {
  const float u = std::clamp(t, 0.0f, 1.0f);
  return u < 0.5f ? 4.0f * u * u * u : 1.0f - std::pow(-2.0f * u + 2.0f, 3.0f) * 0.5f;
}

void Tween::snap(float value) noexcept {
  from_ = value;
  to_ = value;
  length_ = 0.0f;
}

void Tween::to(float target, Timing timing) noexcept {
  from_ = value(timing.now);
  to_ = target;
  start_ = timing.now;
  length_ = timing.milliseconds;
}

float Tween::value(double now) const noexcept {
  if (length_ <= 0.0f) {
    return to_;
  }
  const auto t = static_cast<float>((now - start_) * 1000.0 / static_cast<double>(length_));
  return from_ + (to_ - from_) * easeOut(t);
}

bool Tween::moving(double now) const noexcept {
  return length_ > 0.0f && (now - start_) * 1000.0 < static_cast<double>(length_);
}

void Follow::snap(float value) noexcept {
  value_ = value;
  target_ = value;
}

float Follow::step(Timing timing) noexcept {
  const double now = timing.now;
  const float milliseconds = timing.milliseconds;
  if (last_ < 0.0 || milliseconds <= 0.0f) {
    last_ = now;
    if (milliseconds <= 0.0f) {
      value_ = target_;
    }
    return value_;
  }
  const auto elapsed = static_cast<float>(std::max(0.0, now - last_) * 1000.0);
  last_ = now;
  // A time constant of `milliseconds` closes 63% of the gap in that time.
  const float keep = std::exp(-elapsed / milliseconds);
  value_ = target_ + (value_ - target_) * keep;
  if (std::abs(value_ - target_) < settledWithin) {
    value_ = target_;
  }
  return value_;
}

bool Follow::settled() const noexcept {
  return value_ == target_;
}

void RectFollow::snap(const Rect &rect) noexcept {
  x_.snap(rect.x);
  y_.snap(rect.y);
  width_.snap(rect.width);
  height_.snap(rect.height);
  placed_ = true;
}

void RectFollow::aim(const Rect &rect) noexcept {
  if (!placed_) {
    snap(rect);
    return;
  }
  x_.aim(rect.x);
  y_.aim(rect.y);
  width_.aim(rect.width);
  height_.aim(rect.height);
}

Rect RectFollow::step(Timing timing) noexcept {
  return Rect{x_.step(timing), y_.step(timing), width_.step(timing), height_.step(timing)};
}

Rect RectFollow::value() const noexcept {
  return Rect{x_.value(), y_.value(), width_.value(), height_.value()};
}

bool RectFollow::settled() const noexcept {
  return x_.settled() && y_.settled() && width_.settled() && height_.settled();
}

} // namespace gpu2d::ui
