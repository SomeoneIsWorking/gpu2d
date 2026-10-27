#include "gpu2d/text.hpp"

#include <algorithm>
#include <cmath>

#include "gpu2d/gpu_error.hpp"

namespace gpu2d {
namespace {

std::size_t codepointsIn(std::string_view text) {
  return static_cast<std::size_t>(std::ranges::count_if(text, [](char byte) {
    return (static_cast<unsigned char>(byte) & 0xC0U) != 0x80U;
  }));
}

int quarterPixels(float size) {
  return std::max(static_cast<int>(std::lround(size * 4.0f)), 1);
}

} // namespace

void Text::FontClose::operator()(TTF_Font *font) const noexcept {
  TTF_CloseFont(font);
}

void Text::TextDestroy::operator()(TTF_Text *text) const noexcept {
  TTF_DestroyText(text);
}

void Text::ShaperFaceDestroy::operator()(hb_face_t *face) const noexcept {
  hb_face_destroy(face);
}

void Text::ShaperFontDestroy::operator()(hb_font_t *font) const noexcept {
  hb_font_destroy(font);
}

Text::Text(SDL_GPUDevice &device, const std::filesystem::path &face, FaceOptions options)
    : face_{face}, options_{options} {
  if (!TTF_Init()) {
    throw GpuError{"cannot start SDL_ttf"};
  }
  ttfUp_ = true;
  engine_ = TTF_CreateGPUTextEngine(&device);
  if (engine_ == nullptr) {
    TTF_Quit();
    throw GpuError{"cannot start the GPU text engine"};
  }
  hb_blob_t *blob = hb_blob_create_from_file_or_fail(face.string().c_str());
  if (blob != nullptr) {
    shaperFace_.reset(hb_face_create(blob, 0));
    hb_blob_destroy(blob);
  }
  // Opening one size checks the file now rather than at the first label.
  if (shaperFace_ == nullptr || hb_face_get_glyph_count(shaperFace_.get()) == 0 ||
      font(16.0f) == nullptr) {
    TTF_DestroyGPUTextEngine(engine_);
    TTF_Quit();
    throw GpuError{"cannot read the face " + face.string()};
  }
}

Text::~Text() {
  runs_.clear();
  fonts_.clear();
  TTF_DestroyGPUTextEngine(engine_);
  if (ttfUp_) {
    TTF_Quit();
  }
}

TTF_Font *Text::font(float size) {
  const int key = quarterPixels(size);
  const auto found = fonts_.find(key);
  if (found != fonts_.end()) {
    return found->second.get();
  }
  Font opened{TTF_OpenFont(face_.string().c_str(), static_cast<float>(key) / 4.0f)};
  if (opened == nullptr) {
    return nullptr;
  }
  TTF_SetFontHinting(opened.get(), options_.hinted ? TTF_HINTING_LIGHT : TTF_HINTING_NONE);
  TTF_SetFontKerning(opened.get(), options_.kerned);
  return fonts_.emplace(key, std::move(opened)).first->second.get();
}

LineMetrics Text::metrics(float size) {
  TTF_Font *face = font(size);
  if (face == nullptr) {
    return {};
  }
  return LineMetrics{static_cast<float>(TTF_GetFontAscent(face)),
                     static_cast<float>(-TTF_GetFontDescent(face))};
}

GlyphInk Text::ink(char32_t codepoint, const TextStyle &style) {
  TTF_Font *face = font(style.size);
  int minX = 0;
  int maxX = 0;
  int minY = 0;
  int maxY = 0;
  int advance = 0;
  if (face == nullptr || !TTF_GetGlyphMetrics(face, static_cast<Uint32>(codepoint), &minX, &maxX,
                                              &minY, &maxY, &advance)) {
    return {};
  }
  return GlyphInk{static_cast<float>(minX), static_cast<float>(maxX), static_cast<float>(minY),
                  static_cast<float>(maxY)};
}

float Text::measure(std::string_view text, const TextStyle &style) {
  if (text.empty()) {
    return 0.0f;
  }
  return layout(text, style).advance;
}

hb_font_t *Text::shaper(float size) {
  const int key = quarterPixels(size);
  const auto found = shapers_.find(key);
  if (found != shapers_.end()) {
    return found->second.get();
  }
  ShaperFont made{hb_font_create(shaperFace_.get())};
  // 26.6 fixed point, at the quarter pixel SDL3_ttf opened the face at.
  const int scale = key * 16;
  hb_font_set_scale(made.get(), scale, scale);
  return shapers_.emplace(key, std::move(made)).first->second.get();
}

Text::Layout Text::layout(std::string_view text, const TextStyle &style) {
  std::unique_ptr<hb_buffer_t, decltype(&hb_buffer_destroy)> buffer{hb_buffer_create(),
                                                                    &hb_buffer_destroy};
  hb_buffer_add_utf8(buffer.get(), text.data(), static_cast<int>(text.size()), 0,
                     static_cast<int>(text.size()));
  hb_buffer_guess_segment_properties(buffer.get());
  const hb_feature_t kerning{HB_TAG('k', 'e', 'r', 'n'), options_.kerned ? 1U : 0U,
                             HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END};
  hb_shape(shaper(style.size), buffer.get(), &kerning, 1);
  unsigned int count = 0;
  const hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(buffer.get(), &count);
  const hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(buffer.get(), &count);

  // A cluster runs from its byte offset to the next cluster's, in logical order.
  std::vector<std::size_t> starts;
  starts.reserve(count);
  for (unsigned int i = 0; i < count; ++i) {
    starts.push_back(infos[i].cluster);
  }
  std::ranges::sort(starts);
  const auto clusterEnd = [&](std::size_t start) {
    const auto next = std::ranges::upper_bound(starts, start);
    return next == starts.end() ? text.size() : *next;
  };

  constexpr float unitsPerPixel = 64.0f;
  const float tracking = style.tracking * style.size;
  Layout out;
  float pen = 0.0f;
  for (unsigned int i = 0; i < count; ++i) {
    const std::size_t start = infos[i].cluster;
    if (i == 0 || infos[i - 1].cluster != infos[i].cluster) {
      const auto before = static_cast<float>(codepointsIn(text.substr(0, start)));
      out.characters.push_back(
          Placed{text.substr(start, clusterEnd(start) - start),
                 pen + static_cast<float>(positions[i].x_offset) / unitsPerPixel +
                     tracking * (before + 0.5f)});
    }
    pen += static_cast<float>(positions[i].x_advance) / unitsPerPixel;
  }
  out.advance = pen + tracking * static_cast<float>(codepointsIn(text));
  return out;
}

Text::Run &Text::run(std::string_view cluster, float size) {
  RunKey key{quarterPixels(size), std::string{cluster}};
  const auto found = runs_.find(key);
  if (found != runs_.end()) {
    return found->second;
  }
  Run made;
  TTF_Font *face = font(size);
  if (face != nullptr) {
    made.text.reset(TTF_CreateText(engine_, face, cluster.data(), cluster.size()));
  }
  if (made.text == nullptr) {
    return runs_.emplace(std::move(key), std::move(made)).first->second;
  }
  // SDL3_ttf moves a glyph whose ink starts left of its pen right by that much; undo it so the
  // vertices are relative to the pen.
  int inkLeft = 0;
  int unused = 0;
  const char *bytes = cluster.data();
  std::size_t left = cluster.size();
  const Uint32 first = SDL_StepUTF8(&bytes, &left);
  if (!TTF_GetGlyphMetrics(face, first, &inkLeft, &unused, &unused, &unused, &unused)) {
    inkLeft = 0;
  }
  const auto penShift = static_cast<float>(std::min(inkLeft, 0));
  for (const TTF_GPUAtlasDrawSequence *sequence = TTF_GetGPUTextDrawData(made.text.get());
       sequence != nullptr; sequence = sequence->next) {
    Sequence out;
    out.atlas = sequence->atlas_texture;
    out.vertices.reserve(static_cast<std::size_t>(sequence->num_vertices));
    // SDL3_ttf's positions are y up from the line box's top-left.
    for (int i = 0; i < sequence->num_vertices; ++i) {
      out.vertices.push_back(Vertex{sequence->xy[i].x + penShift, -sequence->xy[i].y,
                                    sequence->uv[i].x, sequence->uv[i].y});
    }
    out.indices.assign(sequence->indices, sequence->indices + sequence->num_indices);
    made.sequences.push_back(std::move(out));
  }
  made.shaped = true;
  return runs_.emplace(std::move(key), std::move(made)).first->second;
}

bool Text::draw(Canvas &canvas, std::string_view text, Point topLeft, const TextStyle &style,
                Colour colour) {
  if (text.empty() || colour.a == 0) {
    return true;
  }
  bool shaped = true;
  std::vector<Vertex> placed;
  for (const Placed &character : layout(text, style).characters) {
    Run &glyphs = run(character.cluster, style.size);
    glyphs.drawn = true;
    if (!glyphs.shaped) {
      shaped = false;
      continue;
    }
    for (const Sequence &sequence : glyphs.sequences) {
      placed.assign(sequence.vertices.begin(), sequence.vertices.end());
      for (Vertex &vertex : placed) {
        vertex.x += topLeft.x + character.pen;
        vertex.y += topLeft.y;
        vertex.r = colour.r;
        vertex.g = colour.g;
        vertex.b = colour.b;
        vertex.a = colour.a;
      }
      canvas.mesh(TextureView{sequence.atlas, 0, 0, Filter::Linear, Alpha::Straight}, placed,
                  sequence.indices);
    }
  }
  return shaped;
}

void Text::retire() {
  std::erase_if(runs_, [](const auto &entry) {
    return !entry.second.drawn;
  });
  for (auto &[key, shaped] : runs_) {
    shaped.drawn = false;
  }
}

} // namespace gpu2d
