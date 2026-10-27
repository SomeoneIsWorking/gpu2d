// tab_dock — tabs along a dock the app draws (a home screen's sections): LB and RB step them from
// anywhere on the screen, or on any screen without tabs of its own while the dock is in the app's
// chrome, wrapping at the ends; a press on a tab chooses it and the pointer lights
// the tab it rests on. Neither the dock nor its tabs take the focus. By default the tabs share the
// dock's frame evenly; an app with its own geometry places them with `place`.
#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "gpu2d/ui/layout.hpp"

namespace gpu2d::ui {

class TabDock : public Container {
public:
  /// How many tabs, and which is chosen.
  struct Tabs {
    std::size_t count{};
    std::size_t chosen{};
  };

  /// `chose` gets the tab chosen, by index; choosing the chosen tab again calls it too.
  TabDock(std::string id, Tabs tabs, std::function<void(std::size_t)> chose);

  /// The id of tab `index`.
  [[nodiscard]] std::string tabId(std::size_t index) const;
  /// The index of the tab of dock `dock` whose id is `id` (a painter's `hovered`), or nothing.
  [[nodiscard]] static std::optional<std::size_t> tabOf(std::string_view dock, std::string_view id);
  [[nodiscard]] std::size_t count() const noexcept {
    return children_.size();
  }
  [[nodiscard]] std::size_t chosen() const noexcept {
    return chosen_;
  }
  /// Chooses the tab `step` places on, wrapping.
  void step(int step);
  /// Chooses tab `index`.
  void choose(std::size_t index);

  void arrange(LayoutContext &context, const Rect &frame) override;
  /// The dock itself takes the pointer, so resting on it between tabs reaches `hover`.
  [[nodiscard]] bool pressable() const override {
    return true;
  }
  /// A press between tabs does nothing.
  bool press(Point at, ActContext &context) override;

protected:
  /// Puts tab `index` at `frame`.
  void place(LayoutContext &context, std::size_t index, const Rect &frame);

private:
  std::size_t chosen_;
  std::function<void(std::size_t)> chose_;
};

} // namespace gpu2d::ui
