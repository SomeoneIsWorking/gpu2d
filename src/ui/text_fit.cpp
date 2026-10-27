#include "gpu2d/ui/text_fit.hpp"

#include <algorithm>
#include <utility>

namespace gpu2d::ui {
namespace {

constexpr std::string_view ellipsis = "\xE2\x80\xA6";

bool continuation(char byte) noexcept {
  return (static_cast<unsigned char>(byte) & 0xC0U) == 0x80U;
}

} // namespace

std::size_t characterStart(std::string_view text, std::size_t at) noexcept {
  if (at >= text.size()) {
    return text.size();
  }
  while (at > 0 && continuation(text[at])) {
    --at;
  }
  return at;
}

std::string fitted(Typesetter &typesetter, std::string_view text, float room, float size) {
  if (typesetter.measure(text, size) <= room) {
    return std::string{text};
  }
  // The longest start, cut at a character boundary, that fits with the ellipsis.
  std::vector<std::size_t> cuts;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (!continuation(text[i])) {
      cuts.push_back(i);
    }
  }
  const auto fits = [&](std::size_t cut) {
    std::string candidate{text.substr(0, cut)};
    candidate += ellipsis;
    return typesetter.measure(candidate, size) <= room;
  };
  std::size_t low = 0;
  std::size_t high = cuts.empty() ? 0 : cuts.size() - 1;
  while (low < high) {
    const std::size_t middle = (low + high + 1) / 2;
    if (fits(cuts[middle])) {
      low = middle;
    } else {
      high = middle - 1;
    }
  }
  std::string kept{text.substr(0, cuts.empty() ? 0 : cuts[low])};
  while (!kept.empty() && kept.back() == ' ') {
    kept.pop_back();
  }
  return kept + std::string{ellipsis};
}

std::string fittedEnd(Typesetter &typesetter, std::string_view text, float room, float size) {
  if (typesetter.measure(text, size) <= room) {
    return std::string{text};
  }
  // The longest end, from a character boundary, that fits after the ellipsis.
  std::vector<std::size_t> cuts;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (!continuation(text[i])) {
      cuts.push_back(i);
    }
  }
  const auto fits = [&](std::size_t cut) {
    std::string candidate{ellipsis};
    candidate += text.substr(cut);
    return typesetter.measure(candidate, size) <= room;
  };
  std::size_t low = 0;
  std::size_t high = cuts.empty() ? 0 : cuts.size() - 1;
  while (low < high) {
    const std::size_t middle = (low + high) / 2;
    if (fits(cuts[middle])) {
      high = middle;
    } else {
      low = middle + 1;
    }
  }
  return std::string{ellipsis} + std::string{text.substr(cuts.empty() ? text.size() : cuts[low])};
}

std::size_t nextCharacter(std::string_view text, std::size_t at) noexcept {
  if (at >= text.size()) {
    return text.size();
  }
  ++at;
  while (at < text.size() && continuation(text[at])) {
    ++at;
  }
  return at;
}

FittedAt fittedAround(Typesetter &typesetter, std::string_view text, std::size_t at, float room,
                      float size) {
  at = characterStart(text, at);
  if (typesetter.measure(text, size) <= room) {
    return FittedAt{std::string{text}, at};
  }
  std::string tail = fittedEnd(typesetter, text, room, size);
  const std::size_t tailFrom = text.size() - (tail.size() - ellipsis.size());
  if (at >= tailFrom) {
    return FittedAt{std::move(tail), ellipsis.size() + at - tailFrom};
  }
  std::string head = fitted(typesetter, text, room, size);
  if (at <= head.size() - ellipsis.size()) {
    return FittedAt{std::move(head), at};
  }
  const float mark = typesetter.measure(ellipsis, size);
  std::string middle = fittedEnd(typesetter, text.substr(0, at), std::max(0.0f, room - mark), size);
  const std::size_t shownAt = middle.size();
  middle += ellipsis;
  return FittedAt{std::move(middle), shownAt};
}

std::vector<std::string> wrapped(Typesetter &typesetter, std::string_view text, float room,
                                 float size) {
  std::vector<std::string> lines;
  std::size_t from = 0;
  while (from <= text.size()) {
    std::size_t stop = text.find('\n', from);
    stop = stop == std::string_view::npos ? text.size() : stop;
    const std::string_view paragraph = text.substr(from, stop - from);
    const std::size_t before = lines.size();
    std::string line;
    std::size_t at = 0;
    while (at <= paragraph.size()) {
      std::size_t end = paragraph.find(' ', at);
      end = end == std::string_view::npos ? paragraph.size() : end;
      const std::string_view word = paragraph.substr(at, end - at);
      std::string joined = line;
      if (!joined.empty() && !word.empty()) {
        joined += ' ';
      }
      joined += word;
      if (!line.empty() && typesetter.measure(joined, size) > room) {
        lines.push_back(line);
        line = std::string{word};
      } else {
        line = std::move(joined);
      }
      at = end + 1;
    }
    if (!line.empty() || (lines.size() == before && stop < text.size())) {
      lines.push_back(line);
    }
    from = stop + 1;
  }
  for (std::string &kept : lines) {
    kept = fitted(typesetter, kept, room, size);
  }
  return lines;
}

} // namespace gpu2d::ui
