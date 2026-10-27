#include "gpu2d/ui/theme.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace gpu2d::ui {
namespace {

std::uint8_t channel(float value) noexcept {
  return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
}

} // namespace

Colour faded(Colour colour, float alpha) noexcept {
  colour.a = channel(static_cast<float>(colour.a) * std::clamp(alpha, 0.0f, 1.0f));
  return colour;
}

Colour mix(Colour from, Colour to, float t) noexcept {
  const float u = std::clamp(t, 0.0f, 1.0f);
  const auto lerp = [u](std::uint8_t a, std::uint8_t b) {
    return channel(static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * u);
  };
  return Colour{lerp(from.r, to.r), lerp(from.g, to.g), lerp(from.b, to.b), lerp(from.a, to.a)};
}

} // namespace gpu2d::ui
