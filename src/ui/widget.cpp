#include "gpu2d/ui/widget.hpp"

#include <algorithm>

namespace gpu2d::ui {

const ViewState::Scroll *ViewState::findScroll(const std::string &id) const {
  const auto found = scrolls_.find(id);
  return found == scrolls_.end() ? nullptr : &found->second;
}

void Painter::label(std::string_view words, Point start, float sizeDp, Colour colour) const {
  if (words.empty()) {
    return;
  }
  text.drawCentred(canvas, words, start, px(sizeDp), fade(colour));
}

float Painter::labelEndingAt(std::string_view words, Point end, float sizeDp, Colour colour) const {
  const float left = end.x - text.measure(words, px(sizeDp));
  label(words, Point{left, end.y}, sizeDp, colour);
  return left;
}

Size Widget::measure(LayoutContext &context, float maxWidth) {
  const Theme &theme = context.theme;
  return Size{fixedWidth_ ? theme.px(*fixedWidth_) : maxWidth,
              fixedHeight_ ? theme.px(*fixedHeight_) : 0.0f};
}

void Widget::arrange(LayoutContext & /*context*/, const Rect &frame) {
  frame_ = frame;
}

float Widget::cornerRadius(const Theme &theme) const {
  return theme.px(theme.shape.smallRadius);
}

bool Widget::act(Action /*action*/, ActContext & /*context*/) {
  return false;
}

bool Widget::press(Point /*at*/, ActContext &context) {
  return act(Action::Accept, context);
}

void Widget::paint(Painter &painter) const {
  paintChildren(painter);
}

void Widget::paintChildren(Painter &painter) const {
  const std::optional<Rect> area = clip();
  const Rect outer = painter.visible;
  if (area) {
    painter.canvas.pushClip(*area);
    painter.visible = intersection(outer, *area);
  }
  for (const std::unique_ptr<Widget> &child : children_) {
    if (intersection(child->frame(), painter.visible).width <= 0.0f) {
      continue;
    }
    child->paint(painter);
  }
  if (area) {
    painter.canvas.popClip();
    painter.visible = outer;
  }
}

bool contains(const Rect &rect, Point point) noexcept {
  return point.x >= rect.x && point.x < rect.right() && point.y >= rect.y &&
         point.y < rect.bottom();
}

Rect intersection(const Rect &a, const Rect &b) noexcept {
  const float left = std::max(a.x, b.x);
  const float top = std::max(a.y, b.y);
  const float right = std::min(a.right(), b.right());
  const float bottom = std::min(a.bottom(), b.bottom());
  if (right <= left || bottom <= top) {
    return Rect{left, top, 0.0f, 0.0f};
  }
  return Rect{left, top, right - left, bottom - top};
}

} // namespace gpu2d::ui
