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

/// A box's extent across the direction.
struct Span {
  float start;
  float end;
};

/// How far aside `there` is from `here` when their centres are `offset` apart across the
/// direction, by how much of the narrower span the other covers: none when one covers it, the
/// centres' offset when they do not overlap, edges that only touch included.
float acrossOf(Span here, Span there, float offset) noexcept {
  const float overlap = std::min(here.end, there.end) - std::max(here.start, there.start);
  const float narrower = std::min(here.end - here.start, there.end - there.start);
  const float covered = narrower > 0.0f ? std::clamp(overlap / narrower, 0.0f, 1.0f) : 0.0f;
  return offset * (1.0f - covered);
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
    const float offset = std::abs(horizontal ? dy : dx);
    const float across = horizontal
                             ? acrossOf({from.y, from.bottom()}, {there.y, there.bottom()}, offset)
                             : acrossOf({from.x, from.right()}, {there.x, there.right()}, offset);
    if (across > coneSlope * along) {
      continue;
    }
    const float gap = std::max(
        0.0f, horizontal ? (sign > 0.0f ? there.x - from.right() : from.x - there.right())
                         : (sign > 0.0f ? there.y - from.bottom() : from.y - there.bottom()));
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
