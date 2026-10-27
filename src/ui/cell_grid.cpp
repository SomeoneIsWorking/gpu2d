#include "gpu2d/ui/cell_grid.hpp"

namespace gpu2d::ui {
namespace {

std::optional<Direction> directionOf(Action action) noexcept {
  switch (action) {
  case Action::Up:
    return Direction::Up;
  case Action::Down:
    return Direction::Down;
  case Action::Left:
    return Direction::Left;
  case Action::Right:
    return Direction::Right;
  default:
    return std::nullopt;
  }
}

} // namespace

bool CellGrid::act(Action action, ActContext &context) {
  const std::optional<Direction> direction = directionOf(action);
  if (!direction) {
    return false;
  }
  // An edge keeps the focus here rather than handing it to whatever lies past the cells.
  if (cells_.move(*direction)) {
    context.sound(Sound::Move);
  }
  return true;
}

bool CellGrid::focusAt(Point at, ActContext &context) {
  const std::optional<Spot> spot = cells_.at(at);
  if (!spot || spot->kind != Spot::Kind::Cell) {
    return false;
  }
  if (cells_.focus(spot->index)) {
    context.sound(Sound::Move);
  }
  return true;
}

bool CellGrid::press(Point at, ActContext &context) {
  const std::optional<Spot> spot = cells_.at(at);
  if (!spot) {
    return false;
  }
  if (spot->kind == Spot::Kind::Page) {
    if (cells_.turn(spot->index)) {
      context.sound(Sound::Move);
    }
    return false;
  }
  focusAt(at, context);
  // The cell's A is the screen's, which sounds it.
  context.perform(Action::Accept);
  return false;
}

void CellGrid::hover(Point at, ActContext &context) {
  focusAt(at, context);
}

bool CellGrid::wheel(float notches, ActContext &context) {
  if (notches == 0.0f) {
    return false;
  }
  if (cells_.step(notches < 0.0f ? 1 : -1)) {
    context.sound(Sound::Move);
  }
  return true;
}

bool CellGrid::context(Point at, ActContext &context) {
  if (!focusAt(at, context)) {
    return true;
  }
  context.perform(Action::Options);
  return true;
}

void CellGrid::paint(Painter &painter) const {
  cells_.paint(painter, frame_);
}

} // namespace gpu2d::ui
