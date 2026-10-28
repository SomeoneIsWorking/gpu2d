// text — a face file shaped by HarfBuzz and rasterised into SDL3_ttf's GPU glyph atlas. Sizes are
// the em in pixels; letter spacing is in ems, added half before and half after each character.
//
// HarfBuzz lays a label out at fractional pen positions and each character is drawn at its own:
// SDL3_ttf's layout puts every glyph on a whole pixel, which at small sizes swallows a narrow
// space. A character's glyphs stay cached while every frame draws them. SDL3_ttf reuses a released
// glyph's atlas cell at once, so they are released only by `retire`, after the frame that last
// drew them was submitted.
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <SDL3/SDL_gpu.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <hb.h>

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
  /// Above zero, each glyph is drawn grown by this many pixels all round (FreeType's stroker,
  /// rounded to whole pixels), in place of the glyph: drawn under the plain label, an outline.
  float outline{};
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
  /// Where `codepoint`'s outline inks at the style's size; all zero for a glyph with no ink or
  /// one the face cannot load.
  [[nodiscard]] GlyphInk ink(char32_t codepoint, const TextStyle &style);
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
  struct ShaperFaceDestroy {
    void operator()(hb_face_t *face) const noexcept;
  };
  struct ShaperFontDestroy {
    void operator()(hb_font_t *font) const noexcept;
  };
  using Font = std::unique_ptr<TTF_Font, FontClose>;
  using ShaperFont = std::unique_ptr<hb_font_t, ShaperFontDestroy>;

  /// One character (a HarfBuzz cluster) of a laid-out label and where its pen stands.
  struct Placed {
    std::string_view cluster;
    float pen{};
  };
  struct Layout {
    std::vector<Placed> characters;
    /// The label's advance, letter spacing included.
    float advance{};
  };

  /// One atlas texture's share of a character: white vertices relative to its pen at the line
  /// box's top.
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
  /// Size in quarter pixels, outline in pixels, then the character.
  using RunKey = std::tuple<int, int, std::string>;

  [[nodiscard]] TTF_Font *font(float size, int outline = 0);
  [[nodiscard]] hb_font_t *shaper(float size);
  [[nodiscard]] Layout layout(std::string_view text, const TextStyle &style);
  [[nodiscard]] Run &run(std::string_view cluster, float size, int outline);

  std::filesystem::path face_;
  FaceOptions options_;
  bool ttfUp_{false};
  TTF_TextEngine *engine_{};
  std::unique_ptr<hb_face_t, ShaperFaceDestroy> shaperFace_;
  /// By size in quarter pixels and outline in pixels.
  std::map<std::pair<int, int>, Font> fonts_;
  std::map<int, ShaperFont> shapers_;
  std::map<RunKey, Run> runs_;
};

} // namespace gpu2d
