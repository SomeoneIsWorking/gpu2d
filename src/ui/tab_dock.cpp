#include "gpu2d/ui/tab_dock.hpp"

#include <charconv>
#include <memory>
#include <utility>

namespace gpu2d::ui {
namespace {

/// One tab: a press chooses it, and the pointer resting on it rests on the dock.
class Tab final : public Widget {
public:
  Tab(std::string id, TabDock &dock, std::size_t index)
      : Widget{std::move(id)}, dock_{dock}, index_{index} {
  }
  [[nodiscard]] bool pressable() const override {
    return true;
  }
  bool press(Point /*at*/, ActContext & /*context*/) override {
    dock_.choose(index_);
    return true;
  }
  void hover(Point at, ActContext &context) override {
    dock_.hover(at, context);
  }

private:
  TabDock &dock_;
  std::size_t index_;
};

} // namespace

TabDock::TabDock(std::string id, Tabs tabs, std::function<void(std::size_t)> chose)
    : Container{std::move(id)}, chosen_{tabs.chosen}, chose_{std::move(chose)} {
  for (std::size_t i = 0; i < tabs.count; ++i) {
    adopt(std::make_unique<Tab>(tabId(i), *this, i));
  }
}

std::string TabDock::tabId(std::size_t index) const {
  return id() + "/" + std::to_string(index);
}

std::optional<std::size_t> TabDock::tabOf(std::string_view dock, std::string_view id) {
  if (id.size() <= dock.size() + 1 || !id.starts_with(dock) || id[dock.size()] != '/') {
    return std::nullopt;
  }
  const std::string_view digits = id.substr(dock.size() + 1);
  std::size_t index{};
  const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), index);
  if (error != std::errc{} || end != digits.data() + digits.size()) {
    return std::nullopt;
  }
  return index;
}

void TabDock::step(int step) {
  const auto size = static_cast<long>(count());
  if (size == 0) {
    return;
  }
  const long next = ((static_cast<long>(chosen_) + step) % size + size) % size;
  choose(static_cast<std::size_t>(next));
}

void TabDock::choose(std::size_t index) {
  if (index >= count()) {
    return;
  }
  chosen_ = index;
  if (chose_) {
    chose_(index);
  }
}

void TabDock::arrange(LayoutContext &context, const Rect &frame) {
  frame_ = frame;
  const std::size_t tabs = count();
  if (tabs == 0) {
    return;
  }
  const Rect inside = inner(context.theme, frame);
  const float gap = context.theme.px(spacing_);
  const float width =
      (inside.width - gap * static_cast<float>(tabs - 1)) / static_cast<float>(tabs);
  for (std::size_t i = 0; i < tabs; ++i) {
    place(context, i,
          Rect{inside.x + static_cast<float>(i) * (width + gap), inside.y, width, inside.height});
  }
}

bool TabDock::press(Point /*at*/, ActContext & /*context*/) {
  return false;
}

void TabDock::place(LayoutContext &context, std::size_t index, const Rect &frame) {
  if (index < children_.size()) {
    children_[index]->arrange(context, frame);
  }
}

} // namespace gpu2d::ui
