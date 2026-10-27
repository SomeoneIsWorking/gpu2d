// Tabs along a dock, driven through the navigator: LB and RB step them from anywhere and wrap, a
// dock in the app's chrome included; a press on a tab chooses it, the pointer lights the tab it
// rests on, and they never take the focus.
#include <cstdio>
#include <memory>
#include <optional>
#include <vector>

#include "../check.hpp"
#include "fakes.hpp"
#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/screen.hpp"
#include "gpu2d/ui/tab_dock.hpp"
#include "gpu2d/ui/widgets.hpp"

namespace {

using namespace gpu2d;
using test::expect;
using ui::Action;

/// A button over a dock of three tabs along the bottom 80 px.
class Home final : public ui::Screen {
public:
  Home() : Screen{"home", "Home"} {
  }
  [[nodiscard]] std::unique_ptr<ui::Widget> build(const ui::Theme & /*theme*/) override {
    auto root = std::make_unique<ui::Column>();
    root->add<ui::Button>("home/play", "Play", [] {
    });
    root->add<ui::Spacer>();
    auto &dock = root->add<ui::TabDock>(
        "home/dock", ui::TabDock::Tabs{.count = 3, .chosen = chosen}, [this](std::size_t index) {
          chosen = index;
          chose.push_back(index);
        });
    dock.fixHeight(80.0f);
    return root;
  }

  std::size_t chosen{0};
  std::vector<std::size_t> chose;
};

void shouldersStepAndWrap() {
  test::Rig rig;
  auto owned = std::make_unique<Home>();
  Home &home = *owned;
  rig.navigator.setRoot(std::move(owned));
  rig.frame();
  rig.navigator.act(Action::NextTab);
  rig.navigator.act(Action::NextTab);
  rig.navigator.act(Action::NextTab);
  expect((home.chose == std::vector<std::size_t>{1, 2, 0}), "RB steps along and wraps");
  rig.navigator.act(Action::PreviousTab);
  expect(home.chosen == 2, "LB wraps back from the first");
  expect(rig.navigator.focused() == "home/play" && rig.moves == 0,
         "the focus stays where it was and the app's hook does the sounding");
}

/// A dock of two tabs as a chrome screen of the app's, over the 80 px at the bottom.
class Dock final : public ui::Screen {
public:
  Dock() : Screen{"dock", "Sections"} {
  }
  [[nodiscard]] std::unique_ptr<ui::Widget> build(const ui::Theme & /*theme*/) override {
    auto root = std::make_unique<ui::Column>();
    root->add<ui::Spacer>();
    root->add<ui::TabDock>("dock/sections", ui::TabDock::Tabs{.count = 2, .chosen = chosen},
                           [this](std::size_t index) {
                             chosen = index;
                           })
        .fixHeight(80.0f);
    return root;
  }

  std::size_t chosen{0};
};

/// A screen with one button and no dock of its own.
class Plain final : public ui::Screen {
public:
  Plain() : Screen{"plain", "Plain"} {
  }
  [[nodiscard]] std::unique_ptr<ui::Widget> build(const ui::Theme & /*theme*/) override {
    auto root = std::make_unique<ui::Column>();
    root->add<ui::Button>("plain/go", "Go", [] {
    });
    return root;
  }
};

void shouldersReachADockInTheChrome() {
  test::Rig rig;
  Dock dock;
  rig.navigator.addChrome(dock);
  rig.navigator.setRoot(std::make_unique<Plain>());
  rig.frame();
  rig.navigator.act(Action::NextTab);
  expect(dock.chosen == 1, "RB steps the chrome's dock when the screen has none");
  rig.navigator.act(Action::NextTab);
  expect(dock.chosen == 0 && rig.navigator.focused() == "plain/go",
         "and wraps, leaving the screen's focus");
}

void thePointerLightsAndChoosesATab() {
  test::Rig rig;
  auto owned = std::make_unique<Home>();
  Home &home = *owned;
  rig.navigator.setRoot(std::move(owned));
  rig.frame();
  const auto *dock = dynamic_cast<const ui::TabDock *>(rig.navigator.find("home/dock"));
  expect(dock != nullptr && dock->count() == 3 && dock->tabId(1) == "home/dock/1",
         "the dock holds its tabs");
  expect(!ui::TabDock::tabOf("home/dock", "other/1") &&
             !ui::TabDock::tabOf("home/dock", "home/docks/1") &&
             ui::TabDock::tabOf("home/dock", "home/dock/2") == std::optional<std::size_t>{2},
         "a tab is known by its id");
  const Rect middle = rig.navigator.find("home/dock/1")->frame();
  const Point on{middle.x + middle.width * 0.5f, middle.y + middle.height * 0.5f};
  rig.navigator.pointer(ui::PointerEvent{ui::PointerKind::Move, on, 0.0f});
  expect(rig.navigator.top()->view().hovered == "home/dock/1",
         "the pointer lights the tab it rests on");
  expect(rig.navigator.focused() == "home/play", "without taking the focus");
  rig.navigator.pointer(ui::PointerEvent{ui::PointerKind::Press, on, 0.0f});
  expect(home.chose == std::vector<std::size_t>{1}, "a press chooses it");
}

} // namespace

int main() {
  shouldersStepAndWrap();
  shouldersReachADockInTheChrome();
  thePointerLightsAndChoosesATab();
  std::printf("ui tab dock: all checks passed\n");
  return 0;
}
