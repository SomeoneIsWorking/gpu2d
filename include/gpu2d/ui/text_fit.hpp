// text_fit — words fitted to a width: cut with an ellipsis or wrapped at spaces, never inside a
// UTF-8 sequence.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "gpu2d/ui/typesetter.hpp"

namespace gpu2d::ui {

/// The largest character boundary of UTF-8 `text` at or before byte `at`.
[[nodiscard]] std::size_t characterStart(std::string_view text, std::size_t at) noexcept;

/// The character boundary of UTF-8 `text` after the character at byte `at`; the end at the end.
[[nodiscard]] std::size_t nextCharacter(std::string_view text, std::size_t at) noexcept;

/// A line fitted to a width with a position in it kept in view.
struct FittedAt {
  std::string text;
  /// The kept position, as a byte offset into `text`.
  std::size_t at{};
};

/// `text` when it fits `room` pixels at `size`, else cut with "…" so byte `at` (a character
/// boundary) shows: its end when `at` is in it, else its start, else the stretch ending at `at`
/// between two ellipses.
[[nodiscard]] FittedAt fittedAround(Typesetter &typesetter, std::string_view text, std::size_t at,
                                    float room, float size);

/// `text` when it fits `room` pixels at `size`, else the longest start of it that fits with "…"
/// after it.
[[nodiscard]] std::string fitted(Typesetter &typesetter, std::string_view text, float room,
                                 float size);

/// `text` when it fits `room` pixels at `size`, else "…" and the longest end of it that fits, as a
/// field shows the end being typed.
[[nodiscard]] std::string fittedEnd(Typesetter &typesetter, std::string_view text, float room,
                                    float size);

/// `text` in lines no wider than `room` at `size`, broken at spaces and at each line break; a word
/// wider than `room` is fitted on a line of its own, and an empty line between breaks is kept.
[[nodiscard]] std::vector<std::string> wrapped(Typesetter &typesetter, std::string_view text,
                                               float room, float size);

} // namespace gpu2d::ui
