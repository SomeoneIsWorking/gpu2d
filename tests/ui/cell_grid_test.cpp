// A grid of cells the app lays out, driven through the navigator: directions move the cell focus
// by the app's rule, the pointer focuses and presses cells and turns pages, a secondary press asks
// for the cell's menu and the wheel steps.
#include <cstdio>
#include <memory>
#include <optional>
#include <vector>

#include "../check.hpp"
#include "fakes.hpp"
#include "gpu2d/ui/cell_grid.hpp"
#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/screen.hpp"

namespace {

using namespace gpu2d;
using test::expect;
using ui::Action;
using ui::CellGrid;

/// Four 100 px cells in a row from the left, and a page control under them turning to page 1.
class Row4 final : public CellGrid::Cells {
public:
  bool move(ui::Direction direction) override {
    const int step = direction == ui::Direction::Right  ? 1
                     : direction == ui::Direction::Left ? -1
                                                        : 0;
    const int next = static_cast<int>(focused) + step;
    if (step == 0 || next < 0 || next > 3) {
      return false;
    }
    focused = static_cast<std::size_t>(next);
    return true;
  }
  bool focus(std::size_t index) override {
    if (index == focused) {
      return false;
    }
    focused = index;
    return true;
  }
  bool turn(std::size_t page) override {
    turned.push_back(page);
    return true;
  }
  bool step(int steps) override {
    steps_.push_back(steps);
    return true;
  }
  [[nodiscard]] std::optional<CellGrid::Spot> at(Point at) const override {
    if (at.y >= 100.0f && at.y < 120.0f) {
      return CellGrid::Spot{CellGrid::Spot::Kind::Page, 1};
    }
    if (at.y < 0.0f || at.y >= 100.0f || at.x < 0.0f || at.x >= 400.0f) {
      return std::nullopt;
    }
    return CellGrid::Spot{CellGrid::Spot::Kind::Cell, static_cast<std::size_t>(at.x / 100.0f)};
  }
  void paint(ui::Painter & /*painter*/, const Rect & /*frame*/) override {
  }

  std::size_t focused{0};
  std::vector<std::size_t> turned;
  std::vector<int> steps_;
};

/// The grid over the whole viewport; A and Options are the screen's.
class Shelf final : public ui::Screen {
public:
  explicit Shelf(Row4 &cells) : Screen{"shelf", "Shelf"}, cells_{cells} {
  }
  [[nodiscard]] std::unique_ptr<ui::Widget> build(const ui::Theme & /*theme*/) override {
    auto root = std::make_unique<ui::Stack>();
    root->add<CellGrid>("shelf/cells", cells_);
    return root;
  }
  bool act(Action action, ui::ActContext & /*context*/) override {
    if (action == Action::Accept || action == Action::Options) {
      done.emplace_back(action, cells_.focused);
      return true;
    }
    return false;
  }

  std::vector<std::pair<Action, std::size_t>> done;

private:
  Row4 &cells_;
};

void directionsFollowTheAppsRule() {
  test::Rig rig;
  Row4 cells;
  auto owned = std::make_unique<Shelf>(cells);
  Shelf &shelf = *owned;
  rig.navigator.setRoot(std::move(owned));
  rig.frame();
  expect(rig.navigator.focused() == "shelf/cells", "the grid holds the focus");
  rig.navigator.act(Action::Right);
  rig.navigator.act(Action::Right);
  expect(cells.focused == 2 && rig.moves == 2, "Right moves the cell focus, sounding each move");
  rig.navigator.act(Action::Up);
  expect(cells.focused == 2 && rig.moves == 2 && rig.navigator.focused() == "shelf/cells",
         "an edge keeps the focus in the grid, silently");
  rig.navigator.act(Action::Accept);
  expect(shelf.done.size() == 1 && shelf.done[0].first == Action::Accept &&
             shelf.done[0].second == 2 && rig.activations == 0,
         "A is the screen's, on the focused cell");
}

void thePointerFocusesPressesAndTurns() {
  test::Rig rig;
  Row4 cells;
  auto owned = std::make_unique<Shelf>(cells);
  Shelf &shelf = *owned;
  rig.navigator.setRoot(std::move(owned));
  rig.frame();
  rig.navigator.pointer(ui::PointerEvent{ui::PointerKind::Move, Point{350.0f, 50.0f}, 0.0f});
  expect(cells.focused == 3 && rig.moves == 1, "moving onto a cell focuses it");
  rig.navigator.pointer(ui::PointerEvent{ui::PointerKind::Move, Point{50.0f, 110.0f}, 0.0f});
  expect(cells.focused == 3 && cells.turned.empty(), "resting on a page control turns nothing");
  rig.navigator.pointer(ui::PointerEvent{ui::PointerKind::Press, Point{50.0f, 110.0f}, 0.0f});
  expect(cells.turned == std::vector<std::size_t>{1} && shelf.done.empty(),
         "a press on a page control turns to its page");
  rig.navigator.pointer(ui::PointerEvent{ui::PointerKind::Press, Point{150.0f, 50.0f}, 0.0f});
  expect(cells.focused == 1 && shelf.done.size() == 1 && shelf.done[0].first == Action::Accept &&
             shelf.done[0].second == 1 && rig.activations == 0,
         "a press on a cell focuses it and is A");
  rig.navigator.pointer(ui::PointerEvent{ui::PointerKind::Context, Point{250.0f, 50.0f}, 0.0f});
  expect(cells.focused == 2 && shelf.done.size() == 2 && shelf.done[1].first == Action::Options &&
             shelf.done[1].second == 2,
         "a secondary press on a cell focuses it and asks for its menu");
  rig.navigator.pointer(ui::PointerEvent{ui::PointerKind::Wheel, Point{900.0f, 700.0f}, -1.0f});
  rig.navigator.pointer(ui::PointerEvent{ui::PointerKind::Wheel, Point{900.0f, 700.0f}, 2.0f});
  expect(cells.steps_ == std::vector<int>{1, -1},
         "the wheel steps the grid anywhere: towards the player is on, away is back");
}

} // namespace

int main() {
  directionsFollowTheAppsRule();
  thePointerFocusesPressesAndTurns();
  std::printf("ui cell grid: all checks passed\n");
  return 0;
}
