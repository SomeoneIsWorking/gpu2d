// focus — which box a direction press moves to among boxes placed freely: the nearest that way
// within a cone. How far aside a box is costs far more than the distance along the direction, so a
// box straight ahead beats a nearer diagonal one. A box that covers the other's span across the
// direction is straight ahead of it, whatever their widths; one that does not overlap it, even
// one whose edge only touches it, is as far aside as its centre.
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
