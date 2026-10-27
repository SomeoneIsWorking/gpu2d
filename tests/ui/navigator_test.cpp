// The screen stack, focus and motion through the navigator, as an app drives them: actions,
// pointer events and the clock.
#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../check.hpp"
#include "fakes.hpp"
#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/screen.hpp"
#include "gpu2d/ui/widgets.hpp"

namespace {

using namespace gpu2d;
using test::expect;
using ui::Action;

/// Tabs down the left; the chosen tab's rows in a list on the right; a hint bar under both.
class Pages final : public ui::Screen {
public:
  explicit Pages(int rows = 30) : Screen{"pages", "Pages"}, rows_{rows} {
  }

  [[nodiscard]] std::unique_ptr<ui::Widget> build(const ui::Theme &theme) override {
    auto root = std::make_unique<ui::Column>();
    auto &panes = root->add<ui::Row>();
    panes.space(theme.spacing.pad);
    panes.align(ui::Align::Stretch).grow();
    std::vector<ui::Tabs::Tab> tabs{{"one", "One"}, {"two", "Two"}, {"three", "Three"}};
    auto &side = panes.add<ui::Tabs>(
        "tabs", tabs, chosen,
        [this](std::size_t index) {
          chosen = index;
        },
        ui::Orientation::Vertical);
    side.fixWidth(theme.spacing.sidebarWidth);
    auto &list = panes.add<ui::List>("rows", theme.spacing.gap);
    list.grow();
    if (heading) {
      list.rows().add<ui::SectionHeading>("Top");
    }
    for (int i = first; i < rows_; ++i) {
      const std::string id = "row" + std::to_string(i);
      list.rows()
          .add<ui::Item>(
              id, ui::Item::Text{"Row " + std::to_string(i), "page " + std::to_string(chosen)})
          .onPress([this, id] {
            pressed.push_back(id);
            if (onPress) {
              onPress();
            }
          });
    }
    bar = &root->add<ui::HintBar>();
    return root;
  }

  BackReply back(ui::ActContext &context) override {
    if (refuseBack) {
      context.sound(ui::Sound::Back);
      ++refused;
      return BackReply::Stay;
    }
    return BackReply::Pop;
  }

  std::size_t chosen{0};
  int first{0};
  bool heading{false};
  bool refuseBack{false};
  int refused{0};
  std::vector<std::string> pressed;
  std::function<void()> onPress;
  /// The bar of the tree last built.
  ui::HintBar *bar{nullptr};

private:
  int rows_;
};

void opensOnTheFirstFocusableAndMovesSpatially() {
  test::Rig rig;
  auto owned = std::make_unique<Pages>();
  Pages &pages = *owned;
  rig.navigator.push(std::move(owned));
  rig.frame();
  expect(rig.navigator.focused() == "tabs/one", "focus opens on the first focusable");
  rig.navigator.act(Action::Down);
  expect(rig.navigator.focused() == "tabs/two" && pages.chosen == 1,
         "moving onto a tab chooses it");
  rig.navigator.act(Action::Right);
  expect(rig.navigator.focused() == "row0", "right enters the rows");
  rig.navigator.act(Action::Down);
  rig.navigator.act(Action::Down);
  expect(rig.navigator.focused() == "row2", "down walks the rows");
  rig.navigator.act(Action::Left);
  expect(rig.navigator.focused() == "tabs/two", "left lands on the chosen tab, not the nearest");
  rig.navigator.act(Action::Right);
  expect(rig.navigator.focused() == "row2", "back into the rows lands where it left them");
  rig.navigator.act(Action::Accept);
  expect(pages.pressed == std::vector<std::string>{"row2"} && rig.activations == 1,
         "A presses the focused row with the activate sound");
  expect(rig.moves >= 5, "each focus move sounds");
}

void focusIsKeptByIdAcrossRebuilds() {
  test::Rig rig;
  auto owned = std::make_unique<Pages>();
  Pages &pages = *owned;
  rig.navigator.push(std::move(owned));
  rig.frame();
  expect(rig.navigator.focus("row5"), "focus moves to a row by id");
  pages.heading = true;
  rig.frame();
  expect(rig.navigator.focused() == "row5", "rows added above keep the focus on its row");
  const float before = rig.navigator.find("row5")->frame().y;
  pages.first = 6;
  rig.frame();
  const std::string landed = rig.navigator.focused();
  expect(landed != "row5" && rig.navigator.find(landed) != nullptr,
         "a focused row that goes away hands the focus on");
  expect(std::abs(rig.navigator.find(landed)->frame().y - before) < 40.0f,
         "to the row now where it was");
}

void scrollEasesTheFocusIntoView() {
  test::Rig rig;
  rig.navigator.push(std::make_unique<Pages>(40));
  rig.frame();
  rig.navigator.focus("row0");
  for (int i = 0; i < 20; ++i) {
    rig.navigator.act(Action::Down);
  }
  rig.frame();
  const Rect half = rig.navigator.find("row20")->frame();
  for (int i = 0; i < 60; ++i) {
    rig.frame(1.0 / 60.0);
  }
  const Rect settled = rig.navigator.find("row20")->frame();
  expect(settled.y < half.y, "the list scrolls towards the focus over time");
  expect(settled.bottom() <= rig.viewport.bottom() - 60.0f + 1.0f,
         "the focused row ends up inside the list's frame");
  expect(!rig.navigator.animating(), "and settles");
}

void backPopsUnlessTheScreenKeepsIt() {
  test::Rig rig;
  auto owned = std::make_unique<Pages>();
  Pages &pages = *owned;
  rig.navigator.push(std::move(owned));
  rig.frame();
  pages.refuseBack = true;
  rig.navigator.act(Action::Back);
  expect(rig.navigator.active() && pages.refused == 1, "a screen may keep itself open on Back");
  pages.refuseBack = false;
  rig.navigator.act(Action::Back);
  expect(!rig.navigator.active() && rig.backs == 2, "Back pops the screen with the back sound");
  expect(rig.navigator.visible(), "a popped screen still fades out");
  rig.frame(1.0);
  expect(!rig.navigator.visible(), "and is gone once faded");
}

void inputGoesToTheTopScreenOnly() {
  test::Rig rig;
  auto lower = std::make_unique<Pages>();
  Pages &bottom = *lower;
  rig.navigator.push(std::move(lower));
  rig.frame();
  auto upper = std::make_unique<Pages>();
  Pages &top = *upper;
  rig.navigator.push(std::move(upper));
  rig.frame();
  rig.navigator.act(Action::Down);
  expect(top.chosen == 1 && bottom.chosen == 0, "only the top screen moves");
  rig.navigator.act(Action::Back);
  expect(rig.navigator.top() == &bottom, "popping shows the one under it");
  expect(rig.navigator.focused() == "tabs/one", "with its own focus as it was");
}

void shouldersStepTheTabsFromAnywhere() {
  test::Rig rig;
  auto owned = std::make_unique<Pages>();
  Pages &pages = *owned;
  rig.navigator.push(std::move(owned));
  rig.frame();
  rig.navigator.act(Action::NextTab);
  expect(pages.chosen == 1 && rig.navigator.focused() == "tabs/two",
         "RB chooses the next tab, the focus going with it from a tab");
  rig.navigator.focus("row3");
  rig.navigator.act(Action::NextTab);
  expect(pages.chosen == 2 && rig.navigator.focused() == "row3",
         "RB from the rows chooses the next tab and leaves the focus in the rows");
  expect(!rig.navigator.act(Action::NextTab), "RB stops at the last tab");
}

void pointerHoversPressesAndScrolls() {
  test::Rig rig;
  auto owned = std::make_unique<Pages>(40);
  Pages &pages = *owned;
  rig.navigator.push(std::move(owned));
  rig.frame();
  const Rect row = rig.navigator.find("row1")->frame();
  const Point middle{row.x + row.width * 0.5f, row.y + row.height * 0.5f};
  rig.navigator.pointer(ui::PointerEvent{ui::PointerKind::Move, middle, 0.0f});
  expect(rig.navigator.focused() == "row1", "hovering focuses what is under the pointer");
  rig.navigator.pointer(ui::PointerEvent{ui::PointerKind::Press, middle, 0.0f});
  expect(pages.pressed == std::vector<std::string>{"row1"}, "a press presses it");
  rig.navigator.pointer(ui::PointerEvent{ui::PointerKind::Wheel, middle, -3.0f});
  for (int i = 0; i < 60; ++i) {
    rig.frame(1.0 / 60.0);
  }
  expect(rig.navigator.find("row1")->frame().y < row.y - 100.0f,
         "the wheel scrolls the list under the pointer");
  expect(rig.navigator.focused() == "row1", "without moving the focus");
  const Rect hidden = rig.navigator.find("row0")->frame();
  rig.navigator.pointer(
      ui::PointerEvent{ui::PointerKind::Press, Point{hidden.x + 10.0f, hidden.y + 10.0f}, 0.0f});
  expect(pages.pressed.size() == 1, "a row scrolled out of its list's frame cannot be pressed");
}

void hintsComeFromTheFocusedWidget() {
  test::Rig rig;
  auto owned = std::make_unique<Pages>();
  rig.navigator.push(std::move(owned));
  rig.frame();
  rig.navigator.focus("row0");
  const std::vector<ui::Hint> hints = rig.navigator.hints();
  expect(hints.size() == 2 && hints[0].keys.front() == Action::Accept &&
             hints[0].label == "Select" && hints[1].keys.front() == Action::Back,
         "the row's A prompt, then the screen's Back");
}

void dialogsConfirmAndCallbacksPushSafely() {
  test::Rig rig;
  auto owned = std::make_unique<Pages>();
  Pages &pages = *owned;
  rig.navigator.push(std::move(owned));
  rig.frame();
  int removed = 0;
  rig.navigator.push(ui::confirm("Remove it?", "It cannot be undone.", "Remove", [&] {
    ++removed;
    // A callback that opens another screen runs before the stack changes under it.
    rig.navigator.push(std::make_unique<Pages>());
  }));
  rig.frame();
  expect(rig.navigator.focused() == "dialog/0", "a confirmation opens on Cancel");
  rig.navigator.act(Action::Right);
  rig.navigator.act(Action::Accept);
  expect(removed == 1, "the confirming button does its one thing");
  const std::vector<const ui::Screen *> screens = rig.navigator.screens();
  expect(screens.size() == 2 && screens[0] == &pages && screens[1]->id() == "pages",
         "the dialog closed and the screen its callback pushed is on top");
  rig.navigator.act(Action::Back);
  rig.navigator.push(ui::confirm("Remove it?", "", "Remove", [&] {
    ++removed;
  }));
  rig.frame();
  rig.navigator.act(Action::Back);
  expect(removed == 1 && rig.navigator.top() == &pages, "Back cancels a confirmation");
}

/// Facts with one button above them: nothing below the button takes focus.
class Facts final : public ui::Screen {
public:
  Facts() : Screen{"facts", "Facts"} {
  }
  [[nodiscard]] std::unique_ptr<ui::Widget> build(const ui::Theme &theme) override {
    auto root = std::make_unique<ui::Column>();
    auto &list = root->add<ui::List>("facts", theme.spacing.gap);
    list.grow();
    list.rows().add<ui::Button>("top", "Top", [] {
    });
    for (int i = 0; i < 40; ++i) {
      list.rows().add<ui::Item>("fact" + std::to_string(i), ui::Item::Text{"Fact", ""});
    }
    return root;
  }
};

void downPastTheLastFocusableScrolls() {
  test::Rig rig;
  rig.navigator.push(std::make_unique<Facts>());
  rig.frame(1.0);
  const float before = rig.navigator.find("fact20")->frame().y;
  expect(rig.navigator.act(Action::Down), "Down with nothing below to focus is still taken");
  rig.frame(1.0);
  rig.frame(1.0);
  const float after = rig.navigator.find("fact20")->frame().y;
  expect(after < before && rig.navigator.focused() == "top",
         "it scrolls the list a row, the focus staying where it is");
  rig.navigator.act(Action::Up);
  rig.frame(1.0);
  rig.frame(1.0);
  expect(rig.navigator.find("fact20")->frame().y == before, "Up scrolls back");
}

void anOverlayNamesItsPromptsOnTheBarBelow() {
  test::Rig rig;
  auto owned = std::make_unique<Pages>();
  Pages &pages = *owned;
  rig.navigator.push(std::move(owned));
  rig.frame();
  rig.navigator.push(ui::confirm("Remove it?", "", "Remove", [] {
  }));
  rig.frame();
  const std::vector<ui::Hint> shown = pages.bar->shown();
  // Back stands first on a bar.
  expect(shown.size() == 2 && shown[0].keys.front() == Action::Back &&
             shown[1].keys.front() == Action::Accept && shown[1].label == "Cancel",
         "the bar under a dialog names what the dialog's focus does");
}

void focusAskedWhileHandlingInputWaits() {
  test::Rig rig;
  auto owned = std::make_unique<Pages>();
  Pages &pages = *owned;
  rig.navigator.push(std::move(owned));
  rig.frame();
  rig.navigator.focus("row0");
  pages.onPress = [&rig] {
    // The row pressed is still running; the focus moves once it is done.
    rig.navigator.focus("row4");
  };
  rig.navigator.act(Action::Accept);
  expect(rig.navigator.focused() == "row4", "the focus asked for during the press lands after it");
}

void ringFollowsTheFocus() {
  test::Rig rig;
  rig.navigator.push(std::make_unique<Pages>());
  rig.frame(1.0);
  rig.navigator.act(Action::Right);
  rig.frame(0.01);
  expect(rig.navigator.animating(), "the ring is on its way to the new focus");
  rig.frame(1.0);
  rig.frame(1.0);
  expect(!rig.navigator.animating(), "and arrives");
}

} // namespace

int main() {
  opensOnTheFirstFocusableAndMovesSpatially();
  focusIsKeptByIdAcrossRebuilds();
  scrollEasesTheFocusIntoView();
  backPopsUnlessTheScreenKeepsIt();
  inputGoesToTheTopScreenOnly();
  shouldersStepTheTabsFromAnywhere();
  pointerHoversPressesAndScrolls();
  hintsComeFromTheFocusedWidget();
  dialogsConfirmAndCallbacksPushSafely();
  ringFollowsTheFocus();
  downPastTheLastFocusableScrolls();
  anOverlayNamesItsPromptsOnTheBarBelow();
  focusAskedWhileHandlingInputWaits();
  std::printf("ui navigator: all checks passed\n");
  return 0;
}
