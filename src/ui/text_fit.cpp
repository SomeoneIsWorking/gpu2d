#include "gpu2d/ui/text_fit.hpp"

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

std::vector<std::string> wrapped(Typesetter &typesetter, std::string_view text, float room,
                                 float size) {
  std::vector<std::string> lines;
  std::string line;
  std::size_t at = 0;
  while (at <= text.size()) {
    std::size_t end = text.find(' ', at);
    end = end == std::string_view::npos ? text.size() : end;
    const std::string_view word = text.substr(at, end - at);
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
  if (!line.empty()) {
    lines.push_back(line);
  }
  for (std::string &kept : lines) {
    kept = fitted(typesetter, kept, room, size);
  }
  return lines;
}

} // namespace gpu2d::ui
