// Layout, text fitting and spatial focus through their public headers, with the fake face.
#include <cstdio>
#include <string>

#include "../check.hpp"
#include "fakes.hpp"
#include "gpu2d/ui/focus.hpp"
#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/text_fit.hpp"
#include "gpu2d/ui/widgets.hpp"

namespace {

using namespace gpu2d;
using test::expect;
using test::near;

struct Layout {
  test::FakeText text;
  ui::Theme theme;
  ui::ViewState state;
  ui::LayoutContext context{theme, text, state, 0.0};
};

/// Whether `text` is whole UTF-8: no sequence cut short.
bool wholeUtf8(const std::string &text) {
  std::size_t i = 0;
  while (i < text.size()) {
    const auto lead = static_cast<unsigned char>(text[i]);
    std::size_t length = 1;
    if ((lead & 0xE0U) == 0xC0U) {
      length = 2;
    } else if ((lead & 0xF0U) == 0xE0U) {
      length = 3;
    } else if ((lead & 0xF8U) == 0xF0U) {
      length = 4;
    } else if ((lead & 0xC0U) == 0x80U) {
      return false;
    }
    if (i + length > text.size()) {
      return false;
    }
    for (std::size_t k = 1; k < length; ++k) {
      if ((static_cast<unsigned char>(text[i + k]) & 0xC0U) != 0x80U) {
        return false;
      }
    }
    i += length;
  }
  return true;
}

void fittingKeepsCharactersWhole() {
  test::FakeText text;
  // "é" and "✓" are two and three bytes; every cut point is tried.
  const std::string words = "Pok\xC3\xA9mon \xE2\x9C\x93 tr\xC3\xA8s long";
  expect(ui::fitted(text, words, 1000.0f, 10.0f) == words, "words that fit are left whole");
  for (int room = 0; room < 105; ++room) {
    const std::string cut = ui::fitted(text, words, static_cast<float>(room), 10.0f);
    expect(wholeUtf8(cut), "a cut never splits a character");
    expect(cut.ends_with("\xE2\x80\xA6"), "a cut ends in an ellipsis");
  }
  const std::string cut = ui::fitted(text, words, 50.0f, 10.0f);
  expect(text.measure(cut, 10.0f) <= 50.0f, "a cut fits its room");
  expect(ui::characterStart(words, 4) == 3, "the boundary before a continuation byte");

  const std::vector<std::string> lines = ui::wrapped(text, "one two three four", 50.0f, 10.0f);
  expect(lines.size() == 2 && lines[0] == "one two" && lines[1] == "three four",
         "wrapping breaks at spaces within the room");
  const std::vector<std::string> paragraphs = ui::wrapped(text, "one two\n\nthree", 50.0f, 10.0f);
  expect(paragraphs == std::vector<std::string>{"one two", "", "three"},
         "a line break starts a line, and an empty line between paragraphs is kept");
}

void columnStacksAndSharesSpareRoom() {
  Layout layout;
  ui::Column column;
  column.pad(ui::Insets::all(10.0f)).space(4.0f);
  ui::Widget &top = column.add<ui::Spacer>(0.0f).fixHeight(30.0f);
  ui::Widget &one = column.add<ui::Spacer>(1.0f);
  ui::Widget &three = column.add<ui::Spacer>(3.0f);
  column.arrange(layout.context, Rect{0.0f, 0.0f, 200.0f, 158.0f});
  near(top.frame().y, 10.0f, "padding above the first child");
  near(top.frame().width, 180.0f, "children span the width inside the padding");
  near(one.frame().y, 44.0f, "spacing between children");
  // 138 inside, 30 fixed, two gaps of 4: 100 to share 1:3.
  near(one.frame().height, 25.0f, "one share of the spare room");
  near(three.frame().height, 75.0f, "three shares of the spare room");
  near(three.frame().bottom(), 148.0f, "the last child ends at the padding");
}

void rowGivesFixedNaturalThenGrowing() {
  Layout layout;
  ui::Row row;
  row.space(10.0f);
  ui::Widget &fixed = row.add<ui::Spacer>(0.0f).fixWidth(100.0f);
  ui::Widget &grows = row.add<ui::Spacer>(1.0f);
  ui::Label &label = row.add<ui::Label>("abcd", ui::TextRole::Body);
  row.arrange(layout.context, Rect{0.0f, 0.0f, 500.0f, 60.0f});
  // The label at 19 px measures 4 * 9.5 = 38.
  near(label.frame().width, 38.0f, "a label takes its natural width");
  near(fixed.frame().width, 100.0f, "a fixed child keeps its width");
  near(grows.frame().width, 500.0f - 100.0f - 38.0f - 20.0f, "a growing child takes the rest");
  near(label.frame().x, 462.0f, "children follow each other with spacing");
  near(label.frame().y, (60.0f - label.frame().height) * 0.5f, "children are centred across");
}

void gridAndCentre() {
  Layout layout;
  ui::Grid grid{"grid", 3};
  grid.space(10.0f);
  for (int i = 0; i < 5; ++i) {
    grid.add<ui::Spacer>(0.0f).fixHeight(40.0f);
  }
  grid.arrange(layout.context, Rect{0.0f, 0.0f, 320.0f, 200.0f});
  const auto cells = grid.children();
  near(cells[1]->frame().x, 110.0f, "equal columns with gaps");
  near(cells[1]->frame().width, 100.0f, "each column a third less the gaps");
  near(cells[4]->frame().y, 50.0f, "the fourth cell starts the second line");

  ui::Centre centre{300.0f};
  ui::Widget &child = centre.add<ui::Spacer>(0.0f).fixHeight(100.0f);
  centre.arrange(layout.context, Rect{0.0f, 0.0f, 1000.0f, 500.0f});
  near(child.frame().x, 350.0f, "centred across, at its widest");
  near(child.frame().y, 200.0f, "centred down");

  ui::Centre tall{300.0f};
  ui::Widget &filling = tall.add<ui::Spacer>();
  tall.pad(ui::Insets::all(10.0f));
  tall.arrange(layout.context, Rect{0.0f, 0.0f, 1000.0f, 500.0f});
  near(filling.frame().y, 10.0f, "a growing child starts at the top");
  near(filling.frame().height, 480.0f, "and takes the whole height");
}

void scrollViewClampsItsOffset() {
  Layout layout;
  ui::ScrollView view{"scroll"};
  auto &column = view.content<ui::Column>();
  for (int i = 0; i < 10; ++i) {
    column.add<ui::Spacer>(0.0f).fixHeight(50.0f);
  }
  layout.state.scroll("scroll").offset.snap(1000.0f);
  layout.state.scroll("scroll").offset.aim(1000.0f);
  view.arrange(layout.context, Rect{0.0f, 100.0f, 300.0f, 200.0f});
  const ui::ViewState::Scroll *scroll = layout.state.findScroll("scroll");
  expect(scroll != nullptr, "the offset is kept by the view's id");
  near(scroll->limit, 300.0f, "it scrolls as far as the content overhangs");
  near(column.children().back()->frame().bottom(), 300.0f,
       "an offset past the end is held at the end");
  expect(view.clip().has_value() && view.clip()->y == 100.0f, "it clips to its frame");
}

void spatialFocusPrefersStraightAhead() {
  const std::vector<Rect> boxes{
      {0.0f, 100.0f, 300.0f, 50.0f},  // straight below, wide
      {320.0f, 60.0f, 50.0f, 50.0f},  // nearer but off to the side
      {0.0f, -100.0f, 300.0f, 50.0f}, // above
  };
  const Rect from{100.0f, 0.0f, 100.0f, 50.0f};
  expect(ui::nearestInDirection(boxes, from, ui::Direction::Down) == 0u,
         "down reaches the box it overlaps across before a nearer diagonal one");
  expect(ui::nearestInDirection(boxes, from, ui::Direction::Up) == 2u, "up reaches the box above");
  expect(!ui::nearestInDirection(boxes, from, ui::Direction::Left).has_value(),
         "nothing to the left");
  expect(ui::nearestTo(boxes, Rect{330.0f, 70.0f, 10.0f, 10.0f}) == 1u,
         "the nearest box by centre");
}

void spatialFocusTouchingIsNotOverlapping() {
  const std::vector<Rect> boxes{
      {0.0f, -80.0f, 40.0f, 40.0f},  // straight above, two steps
      {40.0f, -40.0f, 40.0f, 40.0f}, // up-right, its corner touching
      {60.0f, -10.0f, 40.0f, 40.0f}, // right, a little higher
  };
  const Rect from{0.0f, 0.0f, 40.0f, 40.0f};
  expect(ui::nearestInDirection(boxes, from, ui::Direction::Up) == 0u,
         "a box whose edge only touches is diagonal, not straight ahead");
  expect(ui::nearestInDirection(boxes, from, ui::Direction::Right) == 2u,
         "a box mostly level is straight ahead");
}

} // namespace

int main() {
  fittingKeepsCharactersWhole();
  columnStacksAndSharesSpareRoom();
  rowGivesFixedNaturalThenGrowing();
  gridAndCentre();
  scrollViewClampsItsOffset();
  spatialFocusPrefersStraightAhead();
  spatialFocusTouchingIsNotOverlapping();
  std::printf("ui layout: all checks passed\n");
  return 0;
}
