// Each standard widget's one behaviour, pressed through the navigator as an app would press it.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "../check.hpp"
#include "fakes.hpp"
#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/screen.hpp"
#include "gpu2d/ui/text_fit.hpp"
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
                         })
        .chip("Global", false)
        .onAction(
            Action::Secondary,
            [this] {
              ++followed;
            },
            "Go to Global")
        .onAction(
            Action::Tertiary,
            [this] {
              chosen = 2;
            },
            "Use Carousel");
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
    rows.add<ui::Item>("erase", ui::Item::Text{"Erase saves", ""})
        .onPress([this] {
          ++erased;
        })
        .confirmFirst("Erase saves?", "They cannot be brought back.", "Erase");
    root->add<ui::HintBar>();
    return root;
  }

  bool on{false};
  std::size_t chosen{0};
  int level{50};
  int opened{0};
  int applied{0};
  int followed{0};
  int erased{0};
};

/// A strip of tabs too wide for its frame.
class Strip final : public ui::Screen {
public:
  Strip() : Screen{"strip", "Strip"} {
  }
  [[nodiscard]] std::unique_ptr<ui::Widget> build(const ui::Theme & /*theme*/) override {
    auto root = std::make_unique<ui::Column>();
    std::vector<ui::Tabs::Tab> tabs;
    for (const char *name : {"Global", "Super Nintendo", "Game Boy Advance", "Snes9x", "This game",
                             "Another long name", "And one more"}) {
      tabs.push_back(ui::Tabs::Tab{name, name});
    }
    root->add<ui::Tabs>(
            "scopes", std::move(tabs), chosen,
            [this](std::size_t index) {
              chosen = index;
            },
            ui::Orientation::Horizontal)
        .fixWidth(300.0f);
    return root;
  }
  std::size_t chosen{0};
};

/// Three pictures in a grid of cards, the second chosen now.
class Pictures final : public ui::Screen {
public:
  Pictures() : Screen{"pictures", "Pictures"} {
  }
  [[nodiscard]] std::unique_ptr<ui::Widget> build(const ui::Theme & /*theme*/) override {
    auto root = std::make_unique<ui::Column>();
    auto &grid = root->add<ui::Grid>("grid", 3);
    for (std::size_t i = 0; i < 3; ++i) {
      grid.add<ui::CardButton>(
              "picture/" + std::to_string(i),
              [this, i] {
                picked = i;
              },
              "Choose")
          .mark(i == 1)
          .add<ui::Label>("Picture " + std::to_string(i), ui::TextRole::Caption);
    }
    return root;
  }
  std::size_t picked{9};
};

/// A name and a password typed in, kept here as an app's model would keep them.
class Form final : public ui::Screen {
public:
  Form() : Screen{"form", "Form"} {
  }
  [[nodiscard]] std::unique_ptr<ui::Widget> build(const ui::Theme & /*theme*/) override {
    auto root = std::make_unique<ui::Column>();
    root->add<ui::TextField>("name", "Name", name,
                             [this](std::string text) {
                               name = std::move(text);
                             })
        .placeholder("Your name")
        .limit(8)
        .onSubmit(
            [this] {
              ++submitted;
            },
            "Save");
    root->add<ui::TextField>("password", "Password", password,
                             [this](std::string text) {
                               password = std::move(text);
                             })
        .mask();
    root->add<ui::Button>("ok", "OK", [] {
    });
    return root;
  }
  std::string name;
  std::string password;
  int submitted{0};
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

void rowActionsAndChip() {
  Fixture f;
  f.rig.navigator.focus("choice");
  const std::vector<ui::Hint> hints = f.rig.navigator.hints();
  expect(std::ranges::any_of(hints,
                             [](const ui::Hint &hint) {
                               return hint.label == "Go to Global" &&
                                      hint.keys == std::vector<Action>{Action::Secondary};
                             }),
         "a row with a second action prompts it on X");
  f.rig.navigator.act(Action::Secondary);
  expect(f.board->followed == 1 && f.board->chosen == 0, "X does the second action only");
  f.rig.navigator.act(Action::Tertiary);
  expect(f.board->followed == 1 && f.board->chosen == 2, "Y does the third");
  const auto *choice = dynamic_cast<const ui::Item *>(f.rig.navigator.find("choice"));
  expect(choice != nullptr && choice->chipText() == "Global",
         "a row names where its value is from");
  f.rig.navigator.focus("toggle");
  f.rig.navigator.act(Action::Secondary);
  expect(f.board->followed == 1 && !f.board->on, "X on a row without one does nothing");
}

void confirmFirstAsks() {
  Fixture f;
  f.rig.navigator.focus("erase");
  f.rig.navigator.act(Action::Accept);
  f.rig.frame();
  const ui::Screen *dialog = f.rig.navigator.top();
  expect(dialog != nullptr && dialog->title() == "Erase saves?" && f.board->erased == 0,
         "a row that asks first opens its question");
  expect(f.rig.navigator.focused() == "dialog/1", "with its verb focused");
  f.rig.navigator.act(Action::Back);
  f.rig.frame();
  expect(f.board->erased == 0 && f.rig.navigator.top() == f.board, "Back does nothing");
  f.rig.navigator.act(Action::Accept);
  f.rig.frame();
  f.rig.navigator.act(Action::Accept);
  f.rig.frame();
  expect(f.board->erased == 1 && f.rig.navigator.top() == f.board, "the verb does the press");
}

void wideTabStripScrolls() {
  test::Rig rig;
  auto owned = std::make_unique<Strip>();
  Strip *chosen = owned.get();
  rig.navigator.push(std::move(owned));
  rig.frame();
  expect(rig.navigator.find("scopes")->clip().has_value(), "a strip wider than its frame clips");
  for (int i = 0; i < 6; ++i) {
    rig.navigator.act(Action::NextTab);
  }
  for (int i = 0; i < 40; ++i) {
    rig.frame(0.05);
  }
  const Rect strip = rig.navigator.find("scopes")->frame();
  const Rect last = rig.navigator.find("scopes/And one more")->frame();
  expect(chosen->chosen == 6 && last.right() <= strip.right() + 0.5f && last.x >= strip.x - 0.5f,
         "the chosen tab scrolls whole into view");
}

void cardButtonsChooseInAGrid() {
  test::Rig rig;
  auto owned = std::make_unique<Pictures>();
  Pictures *pictures = owned.get();
  rig.navigator.push(std::move(owned));
  rig.frame();
  expect(rig.navigator.focused() == "picture/0", "the first card takes the focus");
  rig.navigator.act(Action::Right);
  expect(rig.navigator.focused() == "picture/1", "Right moves to the next card");
  const std::vector<ui::Hint> hints = rig.navigator.hints();
  expect(!hints.empty() && hints.front().label == "Choose" &&
             hints.front().keys == std::vector<Action>{Action::Accept},
         "a card prompts its verb on A");
  const auto *card = dynamic_cast<const ui::CardButton *>(rig.navigator.find("picture/1"));
  expect(card != nullptr && card->marked(), "the card chosen now is marked");
  rig.navigator.act(Action::Accept);
  expect(pictures->picked == 1, "A presses the focused card");
  const Rect third = rig.navigator.find("picture/2")->frame();
  rig.navigator.pointer(ui::PointerEvent{
      ui::PointerKind::Press, Point{third.x + third.width * 0.5f, third.y + 2.0f}, 0.0f});
  expect(pictures->picked == 2 && rig.navigator.focused() == "picture/2",
         "a press on a card focuses and presses it");
}

void textFieldsTakeTypedText() {
  test::Rig rig;
  int keyboards = 0;
  rig.navigator.setKeyboard([&keyboards] {
    ++keyboards;
  });
  auto owned = std::make_unique<Form>();
  Form *form = owned.get();
  rig.navigator.push(std::move(owned));
  rig.frame();
  expect(rig.navigator.focused() == "name" && rig.navigator.typing(),
         "a focused field takes typed text");
  const auto *empty = dynamic_cast<const ui::TextField *>(rig.navigator.find("name"));
  expect(empty != nullptr && empty->text().empty(), "the field starts empty");
  expect(rig.navigator.text("Ada") && form->name == "Ada", "typed text goes in at the end");
  rig.navigator.text(" L\xC3\xA9");
  expect(form->name == "Ada L\xC3\xA9", "UTF-8 goes in whole");
  rig.navigator.act(Action::Erase);
  expect(form->name == "Ada L", "Erase takes off the last character, all its bytes");
  rig.navigator.text("ovelace");
  expect(form->name == "Ada L", "text past the limit is refused whole");
  rig.navigator.act(Action::Submit);
  expect(form->submitted == 1, "Submit does the field's own step");
  const std::vector<ui::Hint> hints = rig.navigator.hints();
  expect(hints.size() >= 2 && hints[0].keys == std::vector<Action>{Action::Accept} &&
             hints[1].label == "Save",
         "the field prompts typing and its Submit verb");
  expect(std::ranges::any_of(hints,
                             [](const ui::Hint &hint) {
                               return hint.keys == std::vector<Action>{Action::Tertiary} &&
                                      hint.label == "Clear";
                             }),
         "a field holding text prompts Clear");
  rig.prompts.unboundWhileTyping = {Action::Accept, Action::Tertiary};
  const std::vector<ui::Hint> keyed = rig.navigator.hints();
  expect(!keyed.empty() && keyed[0].label == "Save" &&
             std::ranges::none_of(keyed,
                                  [](const ui::Hint &hint) {
                                    return hint.keys.front() == Action::Accept ||
                                           hint.keys.front() == Action::Tertiary;
                                  }),
         "a hint the device has no control for is not shown");
  rig.prompts.unboundWhileTyping.clear();
  rig.navigator.act(Action::Accept);
  expect(keyboards == 1, "A asks the app's keyboard");
  expect(rig.navigator.act(Action::Tertiary) && form->name.empty(), "Y clears the field");
  expect(std::ranges::none_of(rig.navigator.hints(),
                              [](const ui::Hint &hint) {
                                return hint.label == "Clear";
                              }),
         "an empty field does not prompt Clear");
  expect(!rig.navigator.act(Action::Tertiary), "Y on an empty field falls through");

  rig.navigator.act(Action::Down);
  rig.navigator.text("s3cret");
  const auto *secret = dynamic_cast<const ui::TextField *>(rig.navigator.find("password"));
  expect(form->password == "s3cret" && secret != nullptr && secret->masked(),
         "a masked field takes text like any other");
  expect(!rig.navigator.act(Action::Submit), "Submit with no step of the field's falls through");
  rig.navigator.act(Action::Down);
  expect(!rig.navigator.typing() && !rig.navigator.text("x") && form->password == "s3cret",
         "nothing takes text off a field");
}

void caretMovesWhileEditing() {
  test::Rig rig;
  rig.navigator.setKeyboard([] {
  });
  auto opened = std::make_unique<Form>();
  rig.navigator.push(std::move(opened));
  rig.navigator.edit(true);
  rig.frame();
  expect(rig.navigator.editing(), "an edit asked before the screen is laid out waits for it");
  rig.navigator.pop();
  rig.frame(1.0);
  auto owned = std::make_unique<Form>();
  Form *form = owned.get();
  rig.navigator.push(std::move(owned));
  rig.frame();
  expect(!rig.navigator.act(Action::Left) && !rig.navigator.editing() &&
             rig.navigator.focused() == "name",
         "Left on a field not being edited is a focus move");
  rig.navigator.text("Ad\xC3\xA9");
  expect(rig.navigator.editing(), "typing starts editing");
  rig.navigator.act(Action::Left);
  rig.navigator.act(Action::Left);
  rig.navigator.text("x");
  expect(form->name == "Axd\xC3\xA9",
         "Left steps over a whole character; text goes in at the caret");
  rig.navigator.act(Action::Right);
  rig.navigator.act(Action::Right);
  expect(rig.navigator.act(Action::Right) && rig.navigator.focused() == "name",
         "Right at the end stays in the field");
  rig.navigator.act(Action::Erase);
  expect(form->name == "Axd", "Erase at the end takes the last character, all its bytes");
  rig.navigator.act(Action::Left);
  rig.navigator.act(Action::Erase);
  expect(form->name == "Ad", "Erase takes the character before the caret");
  rig.navigator.text("a");
  expect(form->name == "Aad", "and the caret stays where Erase left it");
  rig.navigator.act(Action::Left);
  rig.navigator.act(Action::Left);
  expect(rig.navigator.act(Action::Left) && rig.navigator.focused() == "name",
         "Left at the start stays in the field");
  rig.navigator.text("_");
  expect(form->name == "_Aad", "typing at the start");

  rig.navigator.act(Action::Down);
  expect(rig.navigator.focused() == "password" && !rig.navigator.editing(),
         "Down leaves the field and its editing");
  rig.navigator.act(Action::Up);
  expect(rig.navigator.focused() == "name" && !rig.navigator.editing(),
         "coming back is not editing");
  rig.navigator.text("!");
  expect(form->name == "_Aad!", "typing again starts at the end");

  rig.navigator.edit(true);
  rig.navigator.act(Action::Left);
  rig.navigator.edit(false);
  rig.navigator.text("?");
  expect(form->name == "_Aad!?", "the app starts and stops editing; a new edit starts at the end");
  rig.navigator.act(Action::Accept);
  rig.navigator.act(Action::Left);
  rig.navigator.act(Action::Submit);
  expect(!rig.navigator.editing() && form->submitted == 1, "Submit stops editing");
}

void fittedAroundKeepsAPlaceInView() {
  // The fake face is half the size a byte wide; at size 2 a byte is a pixel and "…" three.
  test::FakeText text;
  const std::string line = "abcdefghij";
  const ui::FittedAt whole = ui::fittedAround(text, line, 4, 10.0f, 2.0f);
  expect(whole.text == line && whole.at == 4, "a line that fits shows whole");
  const ui::FittedAt end = ui::fittedAround(text, line, 8, 7.0f, 2.0f);
  expect(end.text == "\xE2\x80\xA6"
                     "ghij" &&
             end.at == 5,
         "a place near the end shows the end");
  const ui::FittedAt start = ui::fittedAround(text, line, 2, 7.0f, 2.0f);
  expect(start.text == "abcd\xE2\x80\xA6" && start.at == 2,
         "a place near the start shows the start");
  const ui::FittedAt middle = ui::fittedAround(text, line, 5, 7.0f, 2.0f);
  expect(middle.text == "\xE2\x80\xA6"
                        "e"
                        "\xE2\x80\xA6" &&
             middle.at == 4,
         "a place in the middle shows what ends at it between ellipses");
  expect(ui::nextCharacter("a\xC3\xA9"
                           "b",
                           1) == 3 &&
             ui::nextCharacter("ab", 2) == 2,
         "the next character steps over its bytes and stops at the end");
}

/// A face where one word is a width that float rounding loses when a chevron is added and taken
/// away, noting each cut it is asked to measure.
class RoundingText final : public ui::Typesetter {
public:
  [[nodiscard]] float measure(std::string_view text, float size) override {
    if (text.find("\xE2\x80\xA6") != std::string_view::npos) {
      ++cuts;
    }
    return text == word ? width : fake.measure(text, size);
  }
  [[nodiscard]] float lineHeight(float size) override {
    return fake.lineHeight(size);
  }
  void draw(Canvas & /*canvas*/, std::string_view /*text*/, Point /*topLeft*/, float /*size*/,
            Colour /*colour*/) override {
  }
  void drawOutline(Canvas & /*canvas*/, std::string_view /*text*/, Point /*topLeft*/,
                   float /*size*/, float /*outline*/, Colour /*colour*/) override {
  }

  std::string word{"1 game from Epic"};
  float width{std::nextafter(110.0f, 200.0f)};
  int cuts{0};
  test::FakeText fake;
};

void valueThatSetsTheEndShowsWhole() {
  const ui::Theme theme;
  RoundingText text;
  ui::ViewState state;
  ui::LayoutContext context{theme, text, state};
  const float zone = theme.px(12.0f + theme.spacing.gap * 2.0f);
  expect((text.width + zone) - zone < text.width &&
             (text.width + 2.0f * zone) - 2.0f * zone < text.width,
         "the width loses a bit through the chevrons' sum");
  ui::Item item{"runner", ui::Item::Text{"GE-Proton 11-11", ""}};
  item.value(text.word).onPress(
      [] {
      },
      "Open", true);
  item.arrange(context, Rect{0.0f, 0.0f, 900.0f, 60.0f});
  expect(text.cuts == 0, "a value beside a chevron is not cut when the end was sized to it");
  ui::Choice choice{"runner", ui::Item::Text{"Runner", ""}, std::vector<std::string>{text.word}, 0,
                    [](std::size_t /*index*/) {
                    }};
  choice.arrange(context, Rect{0.0f, 0.0f, 900.0f, 60.0f});
  expect(text.cuts == 0, "nor is the widest choice between its chevrons");
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
  rowActionsAndChip();
  confirmFirstAsks();
  wideTabStripScrolls();
  cardButtonsChooseInAGrid();
  textFieldsTakeTypedText();
  caretMovesWhileEditing();
  fittedAroundKeepsAPlaceInView();
  valueThatSetsTheEndShowsWhole();
  toastComesAndGoes();
  std::printf("ui widgets: all checks passed\n");
  return 0;
}
