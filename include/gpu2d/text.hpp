// text — a face file through SDL3_ttf, FreeType and HarfBuzz, drawn from SDL3_ttf's GPU glyph
// atlas. Sizes are the em in pixels; letter spacing is in ems, added half before and half after
// each character.
//
// A label's shaped glyphs stay cached while every frame draws it. SDL3_ttf reuses a released
// glyph's atlas cell at once, so labels are released only by `retire`, after the frame that last
// drew them was submitted.
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <SDL3/SDL_gpu.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "gpu2d/canvas.hpp"
#include "gpu2d/geometry.hpp"
#include "gpu2d/mesh_pipeline.hpp"

namespace gpu2d {

/// How a face's glyphs are rasterised and spaced.
struct FaceOptions {
  /// FreeType's light hinting; off draws the outlines unhinted.
  bool hinted{true};
  /// The face's kerning pairs.
  bool kerned{true};
};

struct TextStyle {
  /// The em, in pixels.
  float size{};
  /// Letter spacing, in ems.
  float tracking{};
};

/// A face's vertical metrics at one size, in pixels; the line box is ascent to descent.
struct LineMetrics {
  float ascent{};
  /// Below the baseline, positive.
  float descent{};

  [[nodiscard]] float lineBox() const noexcept {
    return ascent + descent;
  }
};

/// A glyph's ink box at one size, in pixels from the pen on the baseline, y growing up.
struct GlyphInk {
  float left{};
  float right{};
  float bottom{};
  float top{};

  [[nodiscard]] float centreX() const noexcept {
    return (left + right) * 0.5f;
  }
  [[nodiscard]] float centreY() const noexcept {
    return (bottom + top) * 0.5f;
  }
};

class Text {
public:
  /// Reads `face` and starts the GPU text engine; throws GpuError when either fails.
  Text(SDL_GPUDevice &device, const std::filesystem::path &face, FaceOptions options = {});
  ~Text();
  Text(const Text &) = delete;
  Text &operator=(const Text &) = delete;
  Text(Text &&) = delete;
  Text &operator=(Text &&) = delete;

  [[nodiscard]] LineMetrics metrics(float size);
  /// Where `codepoint`'s outline inks at `size`; all zero for a glyph with no ink or one the
  /// face cannot load.
  [[nodiscard]] GlyphInk ink(char32_t codepoint, float size);
  /// The width `text` advances, letter spacing included.
  [[nodiscard]] float measure(std::string_view text, const TextStyle &style);
  /// Draws `text` with the top of its line box at `topLeft.y`, starting at `topLeft.x`; false
  /// when SDL_ttf cannot shape it (SDL_GetError says why).
  bool draw(Canvas &canvas, std::string_view text, Point topLeft, const TextStyle &style,
            Colour colour);
  /// Releases the labels not drawn since the last call. Call once per frame, after submitting
  /// the frame.
  void retire();

private:
  struct FontClose {
    void operator()(TTF_Font *font) const noexcept;
  };
  struct TextDestroy {
    void operator()(TTF_Text *text) const noexcept;
  };
  using Font = std::unique_ptr<TTF_Font, FontClose>;

  /// One atlas texture's share of a label: white vertices relative to the line box's top-left.
  struct Sequence {
    SDL_GPUTexture *atlas{};
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
  };
  struct Run {
    std::unique_ptr<TTF_Text, TextDestroy> text;
    std::vector<Sequence> sequences;
    bool drawn{false};
    bool shaped{false};
  };
  /// Size in quarter pixels, tracking in thousandths of an em, then the text.
  using RunKey = std::tuple<int, int, std::string>;

  [[nodiscard]] TTF_Font *font(float size);
  [[nodiscard]] Run &run(std::string_view text, const TextStyle &style);

  std::filesystem::path face_;
  FaceOptions options_;
  bool ttfUp_{false};
  TTF_TextEngine *engine_{};
  std::map<int, Font> fonts_;
  std::map<RunKey, Run> runs_;
};

} // namespace gpu2d
