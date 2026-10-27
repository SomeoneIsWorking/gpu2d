// focus — which box a direction press moves to among boxes placed freely: the nearest that way
// within a cone. The gap between the boxes across the direction costs far more than the distance
// along it, so a box straight ahead beats a nearer diagonal one, and boxes of different widths
// that overlap across the direction are straight ahead of each other.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "gpu2d/geometry.hpp"

namespace gpu2d::ui {

enum class Direction : std::uint8_t { Up, Down, Left, Right };

/// The box `direction` moves to from `from`, or nothing that way.
[[nodiscard]] std::optional<std::size_t>
nearestInDirection(std::span<const Rect> boxes, const Rect &from, Direction direction) noexcept;

/// The box nearest `from` by the distance between centres, or nothing when there are none.
[[nodiscard]] std::optional<std::size_t> nearestTo(std::span<const Rect> boxes,
                                                   const Rect &from) noexcept;

} // namespace gpu2d::ui
