// layout — the containers: a column and a row (children one after another, spare room shared by
// weight), a stack (children over each other), a grid of equal columns and a scroll view (one
// child taller than its frame, clipped, its offset eased and kept by id). Padding and spacing are
// in dp.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gpu2d/ui/widget.hpp"

namespace gpu2d::ui {

class Container : public Widget {
public:
  using Widget::Widget;

  /// Adds a child built from `args`; returns it to set it up further.
  template <typename W, typename... Args> W &add(Args &&...args) {
    return adopt(std::make_unique<W>(std::forward<Args>(args)...));
  }
  /// Adds a child built elsewhere.
  template <typename W> W &add(std::unique_ptr<W> child) {
    return adopt(std::move(child));
  }
  Container &pad(Insets insets) noexcept {
    padding_ = insets;
    return *this;
  }
  Container &space(float dp) noexcept {
    spacing_ = dp;
    return *this;
  }

protected:
  /// The frame less its padding.
  [[nodiscard]] Rect inner(const Theme &theme, const Rect &frame) const noexcept;
  [[nodiscard]] float padX(const Theme &theme) const noexcept;
  [[nodiscard]] float padY(const Theme &theme) const noexcept;
  [[nodiscard]] float gaps(const Theme &theme) const noexcept;

  Insets padding_;
  float spacing_{0.0f};
};

class Column : public Container {
public:
  using Container::Container;
  [[nodiscard]] Size measure(LayoutContext &context, float maxWidth) override;
  void arrange(LayoutContext &context, const Rect &frame) override;
};

enum class Align : std::uint8_t { Start, Centre, End, Stretch };

class Row : public Container {
public:
  using Container::Container;
  /// Where children shorter than the row stand across it.
  Row &align(Align align) noexcept {
    align_ = align;
    return *this;
  }
  /// One child under another across the row's width when it is narrower than `dp` (a portrait
  /// screen); a growing child then shares the height left over.
  Row &stackBelow(float dp) noexcept {
    stackBelowDp_ = dp;
    return *this;
  }
  [[nodiscard]] Size measure(LayoutContext &context, float maxWidth) override;
  void arrange(LayoutContext &context, const Rect &frame) override;

private:
  /// Each child's width in a row `width` pixels wide inside its padding.
  [[nodiscard]] std::vector<float> widths(LayoutContext &context, float width);
  /// Whether the children stand one under another in a row `width` pixels wide.
  [[nodiscard]] bool stacked(const Theme &theme, float width) const;
  void arrangeStacked(LayoutContext &context, const Rect &area);

  Align align_{Align::Centre};
  std::optional<float> stackBelowDp_;
};

/// Buttons at their own widths at the row's end, or one under another across its width when they
/// do not all fit beside each other.
class ButtonRow : public Container {
public:
  using Container::Container;
  [[nodiscard]] Size measure(LayoutContext &context, float maxWidth) override;
  void arrange(LayoutContext &context, const Rect &frame) override;

private:
  /// Each button's own width.
  [[nodiscard]] std::vector<float> widths(LayoutContext &context) const;
};

class Stack : public Container {
public:
  using Container::Container;
  [[nodiscard]] Size measure(LayoutContext &context, float maxWidth) override;
  void arrange(LayoutContext &context, const Rect &frame) override;
};

class Grid : public Container {
public:
  Grid(std::string id, std::size_t columns) : Container{std::move(id)}, columns_{columns} {
  }
  [[nodiscard]] Size measure(LayoutContext &context, float maxWidth) override;
  void arrange(LayoutContext &context, const Rect &frame) override;

private:
  std::size_t columns_;
};

/// A column on a rounded surface.
class Card : public Column {
public:
  using Column::Column;
  /// Drawn as a row is, for a card that stands on another card.
  Card &raised(bool raised = true) noexcept {
    raised_ = raised;
    return *this;
  }
  void paint(Painter &painter) const override;

private:
  bool raised_{false};
};

/// A column on the theme's ground, the root of a screen that covers what is under it.
class Backdrop : public Column {
public:
  using Column::Column;
  void paint(Painter &painter) const override;
};

/// One child `maxWidthDp` wide, centred in its frame; a child that grows takes the frame's whole
/// height. Widened for a descendant (`widenFor`), it is as wide as that one measures with room to
/// spare, plus `extraDp`, when that is wider; never wider than its frame.
class Centre : public Container {
public:
  explicit Centre(float maxWidthDp) : maxWidth_{maxWidthDp} {
  }
  Centre &widenFor(Widget &content, float extraDp) noexcept {
    widenedFor_ = &content;
    widenedBy_ = extraDp;
    return *this;
  }
  [[nodiscard]] Size measure(LayoutContext &context, float maxWidth) override;
  void arrange(LayoutContext &context, const Rect &frame) override;

private:
  /// The child's width in `room` pixels.
  [[nodiscard]] float childWidth(LayoutContext &context, float room);

  float maxWidth_;
  Widget *widenedFor_{nullptr};
  float widenedBy_{0.0f};
};

/// Room that grows; nothing drawn.
class Spacer : public Widget {
public:
  explicit Spacer(float weight = 1.0f) {
    grow(weight);
  }
};

/// One child in a clipped frame, scrolled. Its offset is the screen's, kept by the view's id,
/// which therefore must not be empty; it eases to where the focus or the wheel sends it, and a drag
/// up or down moves it with the pointer. The child stands in from the sides by the focus ring, and
/// from the scroll bar when one shows, so a ring stays in the frame and off the bar.
class ScrollView : public Widget {
public:
  explicit ScrollView(std::string id) : Widget{std::move(id)} {
  }
  template <typename W, typename... Args> W &content(Args &&...args) {
    children_.clear();
    return adopt(std::make_unique<W>(std::forward<Args>(args)...));
  }
  template <typename W> W &content(std::unique_ptr<W> child) {
    children_.clear();
    return adopt(std::move(child));
  }

  [[nodiscard]] Size measure(LayoutContext &context, float maxWidth) override;
  void arrange(LayoutContext &context, const Rect &frame) override;
  [[nodiscard]] std::optional<Rect> clip() const override {
    return frame_;
  }
  [[nodiscard]] bool remembersFocus() const override {
    return true;
  }
  /// Its content, then a bar showing how far it is scrolled while touch controls show.
  void paint(Painter &painter) const override;

private:
  /// The offset and the furthest it scrolls, as laid out.
  float offset_{};
  float limit_{};
};

} // namespace gpu2d::ui
