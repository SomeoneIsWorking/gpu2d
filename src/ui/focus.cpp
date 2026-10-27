#include "gpu2d/ui/focus.hpp"

#include <algorithm>
#include <cmath>

namespace gpu2d::ui {
namespace {

// The gap along the direction counts for little next to the gap across it: Down from a row
// reaches the row under it, not a nearer one off to the side.
constexpr float alongWeight = 0.05f;
// A box more than this many times as far across as along is not that way at all.
constexpr float coneSlope = 2.0f;
// Among boxes that overlap across the direction, the nearer centre wins.
constexpr float centreWeight = 0.01f;

/// The gap between the spans [a0, a1] and [b0, b1]; zero when they overlap.
float spanGap(float a0, float a1, float b0, float b1) noexcept {
  return std::max({0.0f, b0 - a1, a0 - b1});
}

} // namespace

std::optional<std::size_t> nearestInDirection(std::span<const Rect> boxes, const Rect &from,
                                              Direction direction) noexcept {
  const bool horizontal = direction == Direction::Left || direction == Direction::Right;
  const float sign = direction == Direction::Left || direction == Direction::Up ? -1.0f : 1.0f;
  const float fromX = from.x + from.width * 0.5f;
  const float fromY = from.y + from.height * 0.5f;
  std::optional<std::size_t> best;
  float bestScore = 0.0f;
  for (std::size_t i = 0; i < boxes.size(); ++i) {
    const Rect &there = boxes[i];
    const float dx = there.x + there.width * 0.5f - fromX;
    const float dy = there.y + there.height * 0.5f - fromY;
    const float along = sign * (horizontal ? dx : dy);
    if (along <= 0.5f) {
      continue;
    }
    const float across = horizontal ? spanGap(from.y, from.bottom(), there.y, there.bottom())
                                    : spanGap(from.x, from.right(), there.x, there.right());
    if (across > coneSlope * along) {
      continue;
    }
    const float gap = std::max(
        0.0f, horizontal ? (sign > 0.0f ? there.x - from.right() : from.x - there.right())
                         : (sign > 0.0f ? there.y - from.bottom() : from.y - there.bottom()));
    const float offset = std::abs(horizontal ? dy : dx);
    const float score = alongWeight * gap * gap + across * across + centreWeight * offset * offset;
    if (!best || score < bestScore) {
      best = i;
      bestScore = score;
    }
  }
  return best;
}

std::optional<std::size_t> nearestTo(std::span<const Rect> boxes, const Rect &from) noexcept {
  std::optional<std::size_t> best;
  float bestDistance = 0.0f;
  for (std::size_t i = 0; i < boxes.size(); ++i) {
    const float dx = boxes[i].x + boxes[i].width * 0.5f - (from.x + from.width * 0.5f);
    const float dy = boxes[i].y + boxes[i].height * 0.5f - (from.y + from.height * 0.5f);
    const float distance = dx * dx + dy * dy;
    if (!best || distance < bestDistance) {
      best = i;
      bestDistance = distance;
    }
  }
  return best;
}

} // namespace gpu2d::ui
