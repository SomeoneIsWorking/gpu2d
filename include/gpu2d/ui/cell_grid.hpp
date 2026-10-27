// cell_grid — a grid or rail of cells the app lays out, moves and draws (a library's shelf of
// pictures, paged or scrolling): one focusable widget whose cells hold the focus inside it.
// Directions move the cell focus by the app's rule; the pointer focuses the cell it moves onto; a
// press on a cell focuses it and is A, on a page control it turns to that page; a secondary press
// on a cell focuses it and asks for its menu (Options); the wheel steps the app's way. What A and
// Options do is the screen's. The cells draw their own focus.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "gpu2d/ui/focus.hpp"
#include "gpu2d/ui/widget.hpp"

namespace gpu2d::ui {

class CellGrid : public Widget {
public:
  /// What lies under the pointer: a cell, or a control that turns to a page.
  struct Spot {
    enum class Kind : std::uint8_t { Cell, Page };
    Kind kind{Kind::Cell};
    /// The cell, or the page turned to.
    std::size_t index{};
  };

  /// The app's cells: how the focus moves among them, where they lie and how they look.
  class Cells {
  public:
    Cells() = default;
    virtual ~Cells() = default;
    Cells(const Cells &) = delete;
    Cells &operator=(const Cells &) = delete;
    Cells(Cells &&) = delete;
    Cells &operator=(Cells &&) = delete;

    /// Moves the focus one cell `direction`; true when it moved.
    virtual bool move(Direction direction) = 0;
    /// Focuses cell `index`; true when it moved.
    virtual bool focus(std::size_t index) = 0;
    /// Turns to `page`; true when it turned.
    virtual bool turn(std::size_t page) = 0;
    /// One wheel step, 1 towards the end and -1 towards the start; true when the focus moved.
    virtual bool step(int steps) = 0;
    /// What lies under `at`, or nothing.
    [[nodiscard]] virtual std::optional<Spot> at(Point at) const = 0;
    /// Draws the cells in the widget's frame.
    virtual void paint(Painter &painter, const Rect &frame) = 0;
  };

  CellGrid(std::string id, Cells &cells) : Widget{std::move(id)}, cells_{cells} {
  }

  [[nodiscard]] bool focusable() const override {
    return true;
  }
  [[nodiscard]] bool drawsFocus() const override {
    return true;
  }
  bool act(Action action, ActContext &context) override;
  bool press(Point at, ActContext &context) override;
  void hover(Point at, ActContext &context) override;
  bool wheel(float notches, ActContext &context) override;
  bool context(Point at, ActContext &context) override;
  void paint(Painter &painter) const override;

private:
  /// Focuses the cell under `at`, with the move sound when it moved; false when none is there.
  bool focusAt(Point at, ActContext &context);

  Cells &cells_;
};

} // namespace gpu2d::ui
