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
  // Opening one size checks the file now rather than at the first label.
  if (font(16.0f) == nullptr) {
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
  TTF_Font *face = font(style.size);
  int width = 0;
  int height = 0;
  if (face == nullptr || text.empty() ||
      !TTF_GetStringSize(face, text.data(), text.size(), &width, &height)) {
    return 0.0f;
  }
  return static_cast<float>(width) +
         style.tracking * style.size * static_cast<float>(codepointsIn(text));
}

Text::Run &Text::run(std::string_view text, const TextStyle &style) {
  RunKey key{quarterPixels(style.size), static_cast<int>(std::lround(style.tracking * 1000.0f)),
             std::string{text}};
  const auto found = runs_.find(key);
  if (found != runs_.end()) {
    return found->second;
  }
  Run made;
  TTF_Font *face = font(style.size);
  if (face != nullptr) {
    made.text.reset(TTF_CreateText(engine_, face, text.data(), text.size()));
  }
  if (made.text == nullptr) {
    return runs_.emplace(std::move(key), std::move(made)).first->second;
  }
  const float tracking = style.tracking * style.size;
  for (const TTF_GPUAtlasDrawSequence *sequence = TTF_GetGPUTextDrawData(made.text.get());
       sequence != nullptr; sequence = sequence->next) {
    Sequence out;
    out.atlas = sequence->atlas_texture;
    out.vertices.reserve(static_cast<std::size_t>(sequence->num_vertices));
    // SDL3_ttf's positions are y up from the line box's top-left.
    for (int i = 0; i < sequence->num_vertices; ++i) {
      out.vertices.push_back(
          Vertex{sequence->xy[i].x, -sequence->xy[i].y, sequence->uv[i].x, sequence->uv[i].y});
    }
    // Each glyph is a quad of four vertices; spacing moves it by the characters before it.
    for (std::size_t glyph = 0; tracking != 0.0f && glyph + 4 <= out.vertices.size(); glyph += 4) {
      float x = 0.0f;
      float y = 0.0f;
      for (std::size_t corner = glyph; corner < glyph + 4; ++corner) {
        x += out.vertices[corner].x * 0.25f;
        y += out.vertices[corner].y * 0.25f;
      }
      TTF_SubString at{};
      if (!TTF_GetTextSubStringForPoint(made.text.get(), static_cast<int>(x), static_cast<int>(y),
                                        &at)) {
        continue;
      }
      const auto before = static_cast<float>(
          codepointsIn(text.substr(0, static_cast<std::size_t>(std::max(at.offset, 0)))));
      for (std::size_t corner = glyph; corner < glyph + 4; ++corner) {
        out.vertices[corner].x += tracking * (before + 0.5f);
      }
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
  Run &shaped = run(text, style);
  shaped.drawn = true;
  if (!shaped.shaped) {
    return false;
  }
  std::vector<Vertex> placed;
  for (const Sequence &sequence : shaped.sequences) {
    placed.assign(sequence.vertices.begin(), sequence.vertices.end());
    for (Vertex &vertex : placed) {
      vertex.x += topLeft.x;
      vertex.y += topLeft.y;
      vertex.r = colour.r;
      vertex.g = colour.g;
      vertex.b = colour.b;
      vertex.a = colour.a;
    }
    canvas.mesh(TextureView{sequence.atlas, 0, 0, Filter::Linear, Alpha::Straight}, placed,
                sequence.indices);
  }
  return true;
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
