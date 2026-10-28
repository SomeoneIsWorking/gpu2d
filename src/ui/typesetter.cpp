#include "gpu2d/ui/typesetter.hpp"

namespace gpu2d::ui {

namespace {
// Big enough that a hinted face's whole-pixel rounding is lost in the ratio.
constexpr float referenceSize = 64.0f;
} // namespace

void Typesetter::drawCentred(Canvas &canvas, std::string_view text, Point start, float size,
                             Colour colour) {
  if (text.empty()) {
    return;
  }
  draw(canvas, text, Point{start.x, start.y - lineHeight(size) * 0.5f}, size, colour);
}

void Typesetter::drawCentredOutlined(Canvas &canvas, std::string_view text, Point start, float size,
                                     Colour colour, float outline, Colour edge) {
  if (text.empty()) {
    return;
  }
  const Point topLeft{start.x, start.y - lineHeight(size) * 0.5f};
  drawOutline(canvas, text, topLeft, size, outline, edge);
  draw(canvas, text, topLeft, size, colour);
}

float Typesetter::sizeForLineHeight(float height) {
  return height * referenceSize / lineHeight(referenceSize);
}

} // namespace gpu2d::ui
