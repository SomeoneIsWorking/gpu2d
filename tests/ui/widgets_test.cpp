// Each standard widget's one behaviour, pressed through the navigator as an app would press it.
#include <cstdio>
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

/// One of each widget in a list, their values kept here as an app's model would keep them.
class Board final : public ui::Screen {
public:
  Board() : Screen{"board", "Board"} {
  }

  [[nodiscard]] std::unique_ptr<ui::Widget> build(const ui::Theme &theme) override {
    auto root = std::make_unique<ui::Column>();
    auto &list = root->add<ui::List>("list", theme.spacing.gap);
    list.grow();
    ui::Column &rows = list.rows();
    rows.add<ui::SectionHeading>("Switches");
    rows.add<ui::Toggle>("toggle", ui::Item::Text{"Sounds", "Menu sounds"}, on, [this](bool now) {
      on = now;
    });
    rows.add<ui::Choice>("choice", ui::Item::Text{"Layout", ""},
                         std::vector<std::string>{"Grid", "Rail", "Carousel"}, chosen,
                         [this](std::size_t index) {
                           chosen = index;
                         });
    rows.add<ui::Slider>("slider", ui::Item::Text{"Volume", ""}, ui::Slider::Range{0, 100, 5},
                         level,
                         [this](int now) {
                           level = now;
                         })
        .unit("%");
    rows.add<ui::Item>("info", ui::Item::Text{"Version", ""}).value("1.0");
    rows.add<ui::Item>("open", ui::Item::Text{"Folders", ""})
        .value("3")
        .onPress(
            [this] {
              ++opened;
            },
            "Open", true);
    rows.add<ui::Button>("button", "Apply", [this] {
      ++applied;
    });
    root->add<ui::HintBar>();
    return root;
  }

  bool on{false};
  std::size_t chosen{0};
  int level{50};
  int opened{0};
  int applied{0};
};

struct Fixture {
  test::Rig rig;
  Board *board{};

  Fixture() {
    auto owned = std::make_unique<Board>();
    board = owned.get();
    rig.navigator.push(std::move(owned));
    rig.frame();
  }
  [[nodiscard]] Point centreOf(std::string_view id) {
    const Rect frame = rig.navigator.find(id)->frame();
    return Point{frame.x + frame.width * 0.5f, frame.y + frame.height * 0.5f};
  }
};

void headingsAndFactsTakeNoFocus() {
  Fixture f;
  expect(f.rig.navigator.focused() == "toggle", "the heading above is skipped");
  f.rig.navigator.focus("slider");
  f.rig.navigator.act(Action::Down);
  expect(f.rig.navigator.focused() == "open", "an item with nothing to do is skipped");
  const auto *open = dynamic_cast<const ui::Item *>(f.rig.navigator.find("open"));
  expect(open != nullptr && open->valueText() == "3", "an item reads its value");
  f.rig.navigator.act(Action::Accept);
  expect(f.board->opened == 1, "A presses an item that does something");
}

void toggleFlipsOnA() {
  Fixture f;
  f.rig.navigator.act(Action::Accept);
  expect(f.board->on, "A turns a switch on");
  expect(f.rig.navigator.hints().front().label == "Turn off", "its prompt says what A does next");
  f.rig.navigator.act(Action::Right);
  expect(f.board->on && f.rig.navigator.focused() == "toggle",
         "Left and Right leave a switch alone");
  f.rig.navigator.act(Action::Accept);
  expect(!f.board->on, "A turns it off again");
}

void choiceStepsAndOpensAList() {
  Fixture f;
  f.rig.navigator.focus("choice");
  f.rig.navigator.act(Action::Right);
  expect(f.board->chosen == 1, "Right steps to the next option");
  f.rig.navigator.act(Action::Left);
  f.rig.navigator.act(Action::Left);
  expect(f.board->chosen == 2, "Left from the first wraps to the last");
  f.rig.navigator.act(Action::Accept);
  expect(f.rig.navigator.top()->id() == "choice" && f.rig.navigator.focused() == "choice/2",
         "A opens the options as a list on the one chosen");
  f.rig.frame();
  f.rig.navigator.act(Action::Up);
  f.rig.navigator.act(Action::Up);
  f.rig.navigator.act(Action::Accept);
  expect(f.board->chosen == 0 && f.rig.navigator.top() == f.board,
         "A in the list picks and closes it");
  expect(f.rig.navigator.focused() == "choice", "the focus is back on the row");
  f.rig.navigator.act(Action::Accept);
  f.rig.navigator.act(Action::Down);
  f.rig.navigator.act(Action::Back);
  expect(f.board->chosen == 0 && f.rig.navigator.top() == f.board,
         "Back closes the list keeping the choice");
}

void choicePressesStepOrOpen() {
  Fixture f;
  const Rect row = f.rig.navigator.find("choice")->frame();
  const float pad = f.rig.navigator.theme().px(f.rig.navigator.theme().spacing.pad);
  const Point right{row.right() - pad - 4.0f, row.y + row.height * 0.5f};
  f.rig.navigator.pointer(ui::PointerEvent{ui::PointerKind::Press, right, 0.0f});
  expect(f.board->chosen == 1, "a press on the right chevron steps forward");
  f.rig.navigator.pointer(ui::PointerEvent{ui::PointerKind::Press, f.centreOf("choice"), 0.0f});
  expect(f.rig.navigator.top()->id() == "choice", "a press on the row opens the list");
  f.rig.navigator.pointer(ui::PointerEvent{ui::PointerKind::Press, Point{2.0f, 2.0f}, 0.0f});
  expect(f.rig.navigator.top() == f.board, "a press beside the list closes it");
}

void sliderStepsAndTakesPresses() {
  Fixture f;
  f.rig.navigator.focus("slider");
  f.rig.navigator.act(Action::Right);
  expect(f.board->level == 55, "Right steps by the slider's step");
  for (int i = 0; i < 20; ++i) {
    f.rig.navigator.act(Action::Left);
  }
  expect(f.board->level == 0, "it stops at its low end");
  expect(f.rig.navigator.focused() == "slider", "Left at the end does not leave it");
  const Rect row = f.rig.navigator.find("slider")->frame();
  const ui::Theme &theme = f.rig.navigator.theme();
  // The track is the end of the row less the number's room and half a knob each side.
  const float end = row.right() - theme.px(theme.spacing.pad) - theme.px(56.0f) - theme.px(11.0f);
  f.rig.navigator.pointer(
      ui::PointerEvent{ui::PointerKind::Press, Point{end, row.y + row.height * 0.5f}, 0.0f});
  expect(f.board->level == 100, "a press at the track's end sets the top level");
}

void buttonAndHintBarPress() {
  Fixture f;
  f.rig.navigator.focus("button");
  f.rig.navigator.act(Action::Accept);
  expect(f.board->applied == 1, "A presses a button");
  f.rig.navigator.focus("open");
  f.rig.frame();
  // The bar is the root's last child: its Back prompt at the start, the row's A prompt at the end.
  const Rect bar{0.0f, f.rig.viewport.bottom() - 60.0f, f.rig.viewport.width, 60.0f};
  f.rig.navigator.pointer(ui::PointerEvent{
      ui::PointerKind::Press, Point{bar.right() - 30.0f, bar.y + bar.height * 0.5f}, 0.0f});
  expect(f.board->opened == 1, "a press on a prompt does its action");
  f.rig.navigator.pointer(ui::PointerEvent{ui::PointerKind::Press,
                                           Point{bar.x + 30.0f, bar.y + bar.height * 0.5f}, 0.0f});
  expect(!f.rig.navigator.active(), "a press on Back's prompt goes back");
}

void toastComesAndGoes() {
  ui::Toast toast;
  toast.show("Saved", false, 10.0, 3.0);
  expect(toast.visible(10.5) && toast.text() == "Saved", "a toast shows for its time");
  expect(!toast.visible(13.5), "and is gone after");
}

} // namespace

int main() {
  headingsAndFactsTakeNoFocus();
  toggleFlipsOnA();
  choiceStepsAndOpensAList();
  choicePressesStepOrOpen();
  sliderStepsAndTakesPresses();
  buttonAndHintBarPress();
  toastComesAndGoes();
  std::printf("ui widgets: all checks passed\n");
  return 0;
}
