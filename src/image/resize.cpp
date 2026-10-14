#include "gpu2d/image/resize.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace gpu2d::image {
namespace {

constexpr int channels = 4;
/// Both cubics reach two pixels either side of the centre.
constexpr float support = 2.0f;

/// Mitchell-Netravali with B = C = 1/3.
float mitchell(float x) noexcept {
  x = std::abs(x);
  if (x < 1.0f) {
    return (16.0f + x * x * (21.0f * x - 36.0f)) / 18.0f;
  }
  if (x < 2.0f) {
    return (32.0f + x * (-60.0f + x * (36.0f - 7.0f * x))) / 18.0f;
  }
  return 0.0f;
}

/// Catmull-Rom: B = 0, C = 1/2.
float catmullRom(float x) noexcept {
  x = std::abs(x);
  if (x < 1.0f) {
    return 1.0f - x * x * (2.5f - 1.5f * x);
  }
  if (x < 2.0f) {
    return 2.0f - x * (4.0f + x * (0.5f * x - 2.5f));
  }
  return 0.0f;
}

/// The source pixels one output pixel reads along an axis, and their weights.
struct Taps {
  int first{};
  std::vector<float> weights;
};

/// Each output pixel's taps along an axis of `in` pixels scaled to `out`. Distances are measured
/// in the smaller of the two pixel sizes, so a reduction averages every source pixel it covers.
std::vector<Taps> tapsFor(int in, int out) {
  const float scale = static_cast<float>(out) / static_cast<float>(in);
  const bool down = scale < 1.0f;
  const float reach = down ? support / scale : support;
  std::vector<Taps> taps(static_cast<std::size_t>(out));
  for (int o = 0; o < out; ++o) {
    const float centre = (static_cast<float>(o) + 0.5f) / scale;
    const int first = static_cast<int>(std::floor(centre - reach));
    const int last = static_cast<int>(std::ceil(centre + reach));
    Taps &tap = taps[static_cast<std::size_t>(o)];
    tap.first = first;
    float total = 0.0f;
    for (int i = first; i <= last; ++i) {
      const float distance = static_cast<float>(i) + 0.5f - centre;
      const float weight = down ? mitchell(distance * scale) : catmullRom(distance);
      tap.weights.push_back(weight);
      total += weight;
    }
    for (float &weight : tap.weights) {
      weight /= total;
    }
  }
  return taps;
}

/// `index` held inside `[0, size)`: the edge pixels repeat outward.
int clamped(int index, int size) noexcept {
  return std::clamp(index, 0, size - 1);
}

} // namespace

Bitmap resized(const Bitmap &source, int width, int height) {
  if (source.empty() || width <= 0 || height <= 0) {
    return {};
  }
  const std::vector<Taps> across = tapsFor(source.width, width);
  const std::vector<Taps> down = tapsFor(source.height, height);
  const auto stride = static_cast<std::size_t>(width) * channels;

  // Rows first, kept in float so the second pass rounds once.
  std::vector<float> rows(stride * static_cast<std::size_t>(source.height));
  for (int y = 0; y < source.height; ++y) {
    for (int x = 0; x < width; ++x) {
      const Taps &tap = across[static_cast<std::size_t>(x)];
      float *out = rows.data() + static_cast<std::size_t>(y) * stride +
                   static_cast<std::size_t>(x) * channels;
      for (std::size_t k = 0; k < tap.weights.size(); ++k) {
        const std::size_t at =
            source.offset(clamped(tap.first + static_cast<int>(k), source.width), y);
        for (std::size_t c = 0; c < channels; ++c) {
          out[c] += tap.weights[k] * static_cast<float>(source.rgba[at + c]);
        }
      }
    }
  }

  Bitmap out{.width = width, .height = height, .rgba = {}};
  out.rgba.resize(stride * static_cast<std::size_t>(height));
  for (int y = 0; y < height; ++y) {
    const Taps &tap = down[static_cast<std::size_t>(y)];
    for (std::size_t i = 0; i < stride; ++i) {
      float value = 0.0f;
      for (std::size_t k = 0; k < tap.weights.size(); ++k) {
        value +=
            tap.weights[k] *
            rows[static_cast<std::size_t>(clamped(tap.first + static_cast<int>(k), source.height)) *
                     stride +
                 i];
      }
      out.rgba[static_cast<std::size_t>(y) * stride + i] =
          static_cast<std::uint8_t>(std::clamp(value + 0.5f, 0.0f, 255.0f));
    }
  }
  return out;
}

Bitmap fittedWithin(Bitmap source, int longest) {
  const int side = std::max(source.width, source.height);
  if (source.empty() || longest <= 0 || side <= longest) {
    return source;
  }
  const float scale = static_cast<float>(longest) / static_cast<float>(side);
  const auto scaledSide = [scale](int length) {
    return std::max(1, static_cast<int>(static_cast<float>(length) * scale));
  };
  return resized(source, scaledSide(source.width), scaledSide(source.height));
}

} // namespace gpu2d::image
