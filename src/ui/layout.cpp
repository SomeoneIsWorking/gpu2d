#include "gpu2d/ui/layout.hpp"

#include <algorithm>
#include <numeric>

namespace gpu2d::ui {
namespace {

/// The child's height when it is `width` wide: its fixed one or what it measures.
float heightOf(LayoutContext &context, Widget &child, float width) {
  if (const std::optional<float> fixed = child.fixedHeight()) {
    return context.theme.px(*fixed);
  }
  return child.measure(context, width).height;
}

/// The child's width in a column `width` wide: its fixed one or the whole.
float widthIn(const Theme &theme, const Widget &child, float width) {
  const std::optional<float> fixed = child.fixedWidth();
  return fixed ? std::min(theme.px(*fixed), width) : width;
}

} // namespace

Rect Container::inner(const Theme &theme, const Rect &frame) const noexcept {
  return Rect{frame.x + theme.px(padding_.left), frame.y + theme.px(padding_.top),
              std::max(0.0f, frame.width - padX(theme)),
              std::max(0.0f, frame.height - padY(theme))};
}

float Container::padX(const Theme &theme) const noexcept {
  return theme.px(padding_.left + padding_.right);
}

float Container::padY(const Theme &theme) const noexcept {
  return theme.px(padding_.top + padding_.bottom);
}

float Container::gaps(const Theme &theme) const noexcept {
  return children_.empty() ? 0.0f : theme.px(spacing_) * static_cast<float>(children_.size() - 1);
}

Size Column::measure(LayoutContext &context, float maxWidth) {
  const Theme &theme = context.theme;
  const float width = std::max(0.0f, maxWidth - padX(theme));
  float height = gaps(theme);
  float widest = 0.0f;
  for (const std::unique_ptr<Widget> &child : children_) {
    const float childWidth = widthIn(theme, *child, width);
    height += heightOf(context, *child, childWidth);
    widest = std::max(widest,
                      child->fixedWidth() ? childWidth : child->measure(context, childWidth).width);
  }
  const Size own = Widget::measure(context, maxWidth);
  return Size{fixedWidth() ? own.width : std::min(maxWidth, widest + padX(theme)),
              fixedHeight() ? own.height : height + padY(theme)};
}

void Column::arrange(LayoutContext &context, const Rect &frame) {
  frame_ = frame;
  const Theme &theme = context.theme;
  const Rect area = inner(theme, frame);
  std::vector<float> heights;
  heights.reserve(children_.size());
  float used = gaps(theme);
  float weights = 0.0f;
  for (const std::unique_ptr<Widget> &child : children_) {
    // A growing child starts from nothing and takes its share of what is left.
    const float height = child->growth() > 0.0f && !child->fixedHeight()
                             ? 0.0f
                             : heightOf(context, *child, widthIn(theme, *child, area.width));
    heights.push_back(height);
    used += height;
    weights += child->fixedHeight() ? 0.0f : child->growth();
  }
  const float spare = std::max(0.0f, area.height - used);
  float y = area.y;
  for (std::size_t i = 0; i < children_.size(); ++i) {
    Widget &child = *children_[i];
    float height = heights[i];
    if (weights > 0.0f && !child.fixedHeight()) {
      height += spare * child.growth() / weights;
    }
    child.arrange(context, Rect{area.x, y, widthIn(theme, child, area.width), height});
    y += height + theme.px(spacing_);
  }
}

std::vector<float> Row::widths(LayoutContext &context, float width) {
  const Theme &theme = context.theme;
  std::vector<float> widths(children_.size(), 0.0f);
  float used = gaps(theme);
  float weights = 0.0f;
  for (std::size_t i = 0; i < children_.size(); ++i) {
    Widget &child = *children_[i];
    if (const std::optional<float> fixed = child.fixedWidth()) {
      widths[i] = theme.px(*fixed);
    } else if (child.growth() > 0.0f) {
      weights += child.growth();
      continue;
    } else {
      widths[i] = child.measure(context, std::max(0.0f, width - used)).width;
    }
    used += widths[i];
  }
  const float spare = std::max(0.0f, width - used);
  for (std::size_t i = 0; i < children_.size(); ++i) {
    const Widget &child = *children_[i];
    if (weights > 0.0f && !child.fixedWidth() && child.growth() > 0.0f) {
      widths[i] = spare * child.growth() / weights;
    }
  }
  return widths;
}

Size Row::measure(LayoutContext &context, float maxWidth) {
  const Theme &theme = context.theme;
  const float width = std::max(0.0f, maxWidth - padX(theme));
  const std::vector<float> sizes = widths(context, width);
  float height = 0.0f;
  for (std::size_t i = 0; i < children_.size(); ++i) {
    height = std::max(height, heightOf(context, *children_[i], sizes[i]));
  }
  const float total = std::accumulate(sizes.begin(), sizes.end(), gaps(theme));
  const Size own = Widget::measure(context, maxWidth);
  return Size{fixedWidth() ? own.width : std::min(maxWidth, total + padX(theme)),
              fixedHeight() ? own.height : height + padY(theme)};
}

void Row::arrange(LayoutContext &context, const Rect &frame) {
  frame_ = frame;
  const Theme &theme = context.theme;
  const Rect area = inner(theme, frame);
  const std::vector<float> sizes = widths(context, area.width);
  float x = area.x;
  for (std::size_t i = 0; i < children_.size(); ++i) {
    Widget &child = *children_[i];
    float height = area.height;
    float y = area.y;
    if (align_ != Align::Stretch || child.fixedHeight()) {
      height = std::min(area.height, heightOf(context, child, sizes[i]));
      if (align_ == Align::Centre) {
        y += (area.height - height) * 0.5f;
      } else if (align_ == Align::End) {
        y += area.height - height;
      }
    }
    child.arrange(context, Rect{x, y, sizes[i], height});
    x += sizes[i] + theme.px(spacing_);
  }
}

Size Stack::measure(LayoutContext &context, float maxWidth) {
  const Theme &theme = context.theme;
  const float width = std::max(0.0f, maxWidth - padX(theme));
  Size size;
  for (const std::unique_ptr<Widget> &child : children_) {
    const Size measured = child->measure(context, widthIn(theme, *child, width));
    size.width = std::max(size.width, measured.width);
    size.height = std::max(size.height, heightOf(context, *child, widthIn(theme, *child, width)));
  }
  const Size own = Widget::measure(context, maxWidth);
  return Size{fixedWidth() ? own.width : size.width + padX(theme),
              fixedHeight() ? own.height : size.height + padY(theme)};
}

void Stack::arrange(LayoutContext &context, const Rect &frame) {
  frame_ = frame;
  const Rect area = inner(context.theme, frame);
  for (const std::unique_ptr<Widget> &child : children_) {
    child->arrange(context, area);
  }
}

Size Grid::measure(LayoutContext &context, float maxWidth) {
  const Theme &theme = context.theme;
  const float width = std::max(0.0f, maxWidth - padX(theme));
  const std::size_t columns = std::max<std::size_t>(columns_, 1);
  const float gap = theme.px(spacing_);
  const float cell = (width - gap * static_cast<float>(columns - 1)) / static_cast<float>(columns);
  float height = 0.0f;
  for (std::size_t first = 0; first < children_.size(); first += columns) {
    float line = 0.0f;
    for (std::size_t i = first; i < std::min(first + columns, children_.size()); ++i) {
      line = std::max(line, heightOf(context, *children_[i], cell));
    }
    height += line + (first == 0 ? 0.0f : gap);
  }
  return Size{maxWidth, height + padY(theme)};
}

void Grid::arrange(LayoutContext &context, const Rect &frame) {
  frame_ = frame;
  const Theme &theme = context.theme;
  const Rect area = inner(theme, frame);
  const std::size_t columns = std::max<std::size_t>(columns_, 1);
  const float gap = theme.px(spacing_);
  const float cell =
      (area.width - gap * static_cast<float>(columns - 1)) / static_cast<float>(columns);
  float y = area.y;
  for (std::size_t first = 0; first < children_.size(); first += columns) {
    const std::size_t end = std::min(first + columns, children_.size());
    float line = 0.0f;
    for (std::size_t i = first; i < end; ++i) {
      line = std::max(line, heightOf(context, *children_[i], cell));
    }
    for (std::size_t i = first; i < end; ++i) {
      const float x = area.x + static_cast<float>(i - first) * (cell + gap);
      children_[i]->arrange(context, Rect{x, y, cell, line});
    }
    y += line + gap;
  }
}

void Card::paint(Painter &painter) const {
  const Theme &theme = painter.theme;
  if (raised_) {
    painter.canvas.fillRoundRect(frame_, theme.px(theme.shape.radius),
                                 painter.fade(theme.colours.control));
  } else {
    painter.canvas.fillRoundRect(frame_, theme.px(theme.shape.radius) * 1.25f,
                                 painter.fade(theme.colours.surface));
  }
  paintChildren(painter);
}

void Backdrop::paint(Painter &painter) const {
  painter.canvas.fillRect(frame_, painter.fade(painter.theme.colours.ground));
  paintChildren(painter);
}

Size Centre::measure(LayoutContext &context, float maxWidth) {
  const Theme &theme = context.theme;
  const float width = std::min(maxWidth - padX(theme), theme.px(maxWidth_));
  Size size;
  for (const std::unique_ptr<Widget> &child : children_) {
    const Size measured = child->measure(context, width);
    size.width = std::max(size.width, measured.width);
    size.height = std::max(size.height, heightOf(context, *child, width));
  }
  return Size{maxWidth, size.height + padY(theme)};
}

void Centre::arrange(LayoutContext &context, const Rect &frame) {
  frame_ = frame;
  const Theme &theme = context.theme;
  const Rect area = inner(theme, frame);
  const float width = std::min(area.width, theme.px(maxWidth_));
  for (const std::unique_ptr<Widget> &child : children_) {
    const float height = child->growth() > 0.0f
                             ? area.height
                             : std::min(area.height, heightOf(context, *child, width));
    child->arrange(context, Rect{area.x + (area.width - width) * 0.5f,
                                 area.y + (area.height - height) * 0.5f, width, height});
  }
}

Size ScrollView::measure(LayoutContext &context, float maxWidth) {
  if (children_.empty()) {
    return Widget::measure(context, maxWidth);
  }
  const Size content = children_.front()->measure(context, maxWidth);
  const Size own = Widget::measure(context, maxWidth);
  return Size{maxWidth, fixedHeight() ? own.height : content.height};
}

void ScrollView::arrange(LayoutContext &context, const Rect &frame) {
  frame_ = frame;
  ViewState::Scroll &scroll = context.state.scroll(id());
  if (children_.empty()) {
    scroll.limit = 0.0f;
    return;
  }
  Widget &content = *children_.front();
  const float height = heightOf(context, content, frame.width);
  scroll.viewport = frame.height;
  scroll.limit = std::max(0.0f, height - frame.height);
  scroll.offset.aim(std::clamp(scroll.offset.target(), 0.0f, scroll.limit));
  const float offset = std::clamp(
      scroll.offset.step(Timing{context.now, context.theme.motion.scroll}), 0.0f, scroll.limit);
  content.arrange(context,
                  Rect{frame.x, frame.y - offset, frame.width, std::max(height, frame.height)});
}

void ScrollView::paint(Painter &painter) const {
  paintChildren(painter);
}

} // namespace gpu2d::ui
