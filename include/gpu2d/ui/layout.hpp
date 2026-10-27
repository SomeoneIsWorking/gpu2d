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
  [[nodiscard]] Size measure(LayoutContext &context, float maxWidth) override;
  void arrange(LayoutContext &context, const Rect &frame) override;

private:
  /// Each child's width in a row `width` pixels wide inside its padding.
  [[nodiscard]] std::vector<float> widths(LayoutContext &context, float width);

  Align align_{Align::Centre};
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
  void paint(Painter &painter) const override;
};

/// A column on the theme's ground, the root of a screen that covers what is under it.
class Backdrop : public Column {
public:
  using Column::Column;
  void paint(Painter &painter) const override;
};

/// One child at the size it measures (no wider than `maxWidthDp`), centred in its frame.
class Centre : public Container {
public:
  explicit Centre(float maxWidthDp) : maxWidth_{maxWidthDp} {
  }
  [[nodiscard]] Size measure(LayoutContext &context, float maxWidth) override;
  void arrange(LayoutContext &context, const Rect &frame) override;

private:
  float maxWidth_;
};

/// Room that grows; nothing drawn.
class Spacer : public Widget {
public:
  explicit Spacer(float weight = 1.0f) {
    grow(weight);
  }
};

/// One child in a clipped frame, scrolled. Its offset is the screen's, kept by the view's id,
/// which therefore must not be empty; it eases to where the focus or the wheel sends it.
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
  void paint(Painter &painter) const override;
};

} // namespace gpu2d::ui
