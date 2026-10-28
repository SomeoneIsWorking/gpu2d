#include "gpu2d/ui/navigator.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

#include "gpu2d/ui/focus.hpp"
#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/tab_dock.hpp"
#include "gpu2d/ui/widgets.hpp"

namespace gpu2d::ui {
namespace {

/// How far a screen slides as it comes and goes, in dp.
constexpr float slideDp = 28.0f;
/// How far an overlay rises as it comes, in dp.
constexpr float riseDp = 14.0f;
/// A wheel notch scrolls this share of a row.
constexpr float wheelRows = 1.0f;

/// A focusable widget with the groups around it, outermost first, and what of it can be seen.
struct Focusable {
  Widget *widget{};
  std::vector<Widget *> groups;
  Rect visible;
};

void collect(Widget &widget, std::vector<Widget *> &groups, const Rect &visible,
             std::vector<Focusable> &out) {
  const std::optional<Rect> clip = widget.clip();
  const Rect inside = clip ? intersection(visible, *clip) : visible;
  const bool group = widget.remembersFocus() && !widget.id().empty();
  if (group) {
    groups.push_back(&widget);
  }
  if (widget.focusable() && !widget.id().empty()) {
    out.push_back(Focusable{&widget, groups, intersection(widget.frame(), inside)});
  }
  for (const std::unique_ptr<Widget> &child : widget.children()) {
    collect(*child, groups, inside, out);
  }
  if (group) {
    groups.pop_back();
  }
}

std::vector<Focusable> focusables(Widget &root, const Rect &viewport) {
  std::vector<Focusable> out;
  std::vector<Widget *> groups;
  collect(root, groups, viewport, out);
  return out;
}

Widget *findIn(Widget &widget, std::string_view id) {
  if (widget.id() == id && !id.empty()) {
    return &widget;
  }
  for (const std::unique_ptr<Widget> &child : widget.children()) {
    if (Widget *found = findIn(*child, id)) {
      return found;
    }
  }
  return nullptr;
}

/// The widgets from `widget` down to the one `id`, or empty when it is not under it.
bool pathTo(Widget &widget, std::string_view id, std::vector<Widget *> &path) {
  path.push_back(&widget);
  if (widget.id() == id) {
    return true;
  }
  for (const std::unique_ptr<Widget> &child : widget.children()) {
    if (pathTo(*child, id, path)) {
      return true;
    }
  }
  path.pop_back();
  return false;
}

/// The deepest focusable or pressable widget under `at` and inside every clip above it.
Widget *hitTest(Widget &widget, Point at) {
  if (const std::optional<Rect> clip = widget.clip(); clip && !contains(*clip, at)) {
    return nullptr;
  }
  const auto children = widget.children();
  for (auto child = children.rbegin(); child != children.rend(); ++child) {
    if (Widget *hit = hitTest(**child, at)) {
      return hit;
    }
  }
  if ((widget.focusable() || widget.pressable()) && contains(widget.frame(), at)) {
    return &widget;
  }
  return nullptr;
}

/// The innermost scroll view under `at` that can scroll.
ScrollView *scrollAt(Widget &widget, Point at, ViewState &view) {
  if (const std::optional<Rect> clip = widget.clip(); clip && !contains(*clip, at)) {
    return nullptr;
  }
  for (const std::unique_ptr<Widget> &child : widget.children()) {
    if (ScrollView *inner = scrollAt(*child, at, view)) {
      return inner;
    }
  }
  auto *scroll = dynamic_cast<ScrollView *>(&widget);
  if (scroll != nullptr && contains(widget.frame(), at)) {
    const ViewState::Scroll *state = view.findScroll(scroll->id());
    if (state != nullptr && state->limit > 0.0f) {
      return scroll;
    }
  }
  return nullptr;
}

/// The first scroll view under `widget` that can scroll.
ScrollView *firstScroll(Widget &widget, ViewState &view) {
  if (auto *scroll = dynamic_cast<ScrollView *>(&widget)) {
    const ViewState::Scroll *state = view.findScroll(scroll->id());
    if (state != nullptr && state->limit > 0.0f) {
      return scroll;
    }
  }
  for (const std::unique_ptr<Widget> &child : widget.children()) {
    if (ScrollView *found = firstScroll(*child, view)) {
      return found;
    }
  }
  return nullptr;
}

Tabs *findTabs(Widget &widget) {
  if (auto *tabs = dynamic_cast<Tabs *>(&widget)) {
    return tabs;
  }
  for (const std::unique_ptr<Widget> &child : widget.children()) {
    if (Tabs *found = findTabs(*child)) {
      return found;
    }
  }
  return nullptr;
}

TabDock *findDock(Widget &widget) {
  if (auto *dock = dynamic_cast<TabDock *>(&widget)) {
    return dock;
  }
  for (const std::unique_ptr<Widget> &child : widget.children()) {
    if (TabDock *found = findDock(*child)) {
      return found;
    }
  }
  return nullptr;
}

void findBars(Widget &widget, std::vector<HintBar *> &bars) {
  if (auto *bar = dynamic_cast<HintBar *>(&widget)) {
    bars.push_back(bar);
  }
  for (const std::unique_ptr<Widget> &child : widget.children()) {
    findBars(*child, bars);
  }
}

Rect grown(const Rect &rect, float by) {
  return Rect{rect.x - by, rect.y - by, rect.width + 2.0f * by, rect.height + 2.0f * by};
}

} // namespace

class Navigator::Context final : public ActContext {
public:
  Context(Navigator &navigator, Screen &screen) : navigator_{navigator}, screen_{screen} {
  }
  void push(std::unique_ptr<Screen> screen) override {
    navigator_.push(std::move(screen));
  }
  void pop() override {
    screen_.close();
  }
  void sound(Sound sound) override {
    switch (sound) {
    case Sound::Move:
      navigator_.play(navigator_.sounds_.move);
      break;
    case Sound::Activate:
      navigator_.play(navigator_.sounds_.activate);
      break;
    case Sound::Back:
      navigator_.play(navigator_.sounds_.back);
      break;
    }
  }
  void perform(Action action) override {
    performed.push_back(action);
  }
  void focus(std::string id) override {
    focusTo = std::move(id);
  }
  void openKeyboard() override {
    if (navigator_.keyboard_) {
      navigator_.keyboard_();
    }
  }
  [[nodiscard]] double now() const override {
    return navigator_.now_;
  }
  [[nodiscard]] const Theme &theme() const override {
    return navigator_.theme_;
  }

  std::vector<Action> performed;
  std::optional<std::string> focusTo;

private:
  Navigator &navigator_;
  Screen &screen_;
};

void Navigator::setRoot(std::unique_ptr<Screen> root) {
  Entry entry;
  entry.screen = std::move(root);
  entry.presence.snap(1.0f);
  if (root_) {
    stack_.front() = std::move(entry);
  } else {
    stack_.insert(stack_.begin(), std::move(entry));
    root_ = true;
  }
  if (viewport_.width > 0.0f) {
    build(stack_.front());
  }
}

void Navigator::push(std::unique_ptr<Screen> screen) {
  if (busy_ > 0) {
    deferred_.push_back(Deferred{Deferred::Kind::Push, std::move(screen)});
    return;
  }
  Entry entry;
  entry.screen = std::move(screen);
  entry.presence.snap(0.0f);
  entry.presence.to(1.0f, Timing{now_, theme_.motion.screen});
  stack_.push_back(std::move(entry));
  if (viewport_.width <= 0.0f) {
    return;
  }
  build(stack_.back());
}

void Navigator::pop() {
  if (busy_ > 0) {
    deferred_.push_back(Deferred{Deferred::Kind::Pop, nullptr});
    return;
  }
  if (stack_.size() <= base()) {
    return;
  }
  Entry entry = std::move(stack_.back());
  stack_.pop_back();
  entry.presence.to(0.0f, Timing{now_, theme_.motion.screen});
  entry.leaving = true;
  leaving_.push_back(std::move(entry));
}

void Navigator::replace(std::unique_ptr<Screen> screen) {
  if (busy_ > 0) {
    deferred_.push_back(Deferred{Deferred::Kind::Replace, std::move(screen)});
    return;
  }
  pop();
  push(std::move(screen));
}

void Navigator::clear() {
  if (busy_ > 0) {
    deferred_.push_back(Deferred{Deferred::Kind::Clear, nullptr});
    return;
  }
  while (stack_.size() > base()) {
    pop();
  }
}

void Navigator::closeFrom(const Screen &screen) {
  const auto at = std::ranges::find_if(stack_, [&screen](const Entry &entry) {
    return entry.screen.get() == &screen;
  });
  for (auto entry = at; entry != stack_.end(); ++entry) {
    entry->screen->close();
  }
  if (busy_ == 0) {
    dropClosing();
  }
}

void Navigator::dropClosing() {
  for (std::size_t i = stack_.size(); i-- > base();) {
    if (stack_[i].screen->closing()) {
      Entry entry = std::move(stack_[i]);
      stack_.erase(stack_.begin() + static_cast<std::ptrdiff_t>(i));
      entry.presence.to(0.0f, Timing{now_, theme_.motion.screen});
      entry.leaving = true;
      leaving_.push_back(std::move(entry));
    }
  }
}

bool Navigator::visible() const noexcept {
  return stack_.size() > base() || !leaving_.empty();
}

bool Navigator::pagesVisible() const noexcept {
  const auto page = [](const Entry &entry) {
    return !entry.screen->overlay();
  };
  const auto above = stack_.begin() + static_cast<std::ptrdiff_t>(std::min(base(), stack_.size()));
  return std::any_of(above, stack_.end(), page) || std::ranges::any_of(leaving_, page);
}

bool Navigator::animating() const noexcept {
  const auto moving = [this](const Entry &entry) {
    return entry.presence.moving(now_);
  };
  if (!leaving_.empty() || std::ranges::any_of(stack_, moving)) {
    return true;
  }
  return !stack_.empty() && !stack_.back().screen->view().ring.settled();
}

Screen *Navigator::top() noexcept {
  return stack_.empty() ? nullptr : stack_.back().screen.get();
}

const Screen *Navigator::top() const noexcept {
  return stack_.empty() ? nullptr : stack_.back().screen.get();
}

std::vector<const Screen *> Navigator::screens() const {
  std::vector<const Screen *> out;
  out.reserve(stack_.size());
  for (const Entry &entry : stack_) {
    out.push_back(entry.screen.get());
  }
  return out;
}

void Navigator::addChrome(Screen &chrome) {
  chrome_.push_back(Chrome{&chrome, nullptr});
}

void Navigator::paintChrome(Canvas &canvas, const Screen &chrome) {
  for (const Chrome &entry : chrome_) {
    if (entry.screen != &chrome || entry.root == nullptr) {
      continue;
    }
    const ViewState &view = chrome.view();
    Painter painter{canvas,       theme_, text_, prompts_, view.focused,
                    view.hovered, now_,   1.0f,  viewport_};
    entry.root->paint(painter);
  }
}

void Navigator::build(Entry &entry) {
  ViewState &view = entry.screen->view();
  LayoutContext context{theme_, text_, view, now_};
  entry.root = entry.screen->build(theme_);
  Rect room = viewport_;
  if (entry.screen->overlay()) {
    room.height = std::max(room.height - covered_, 0.0f);
  }
  entry.area = entry.screen->area(room, theme_);
  entry.root->arrange(context, entry.area);

  const std::vector<Focusable> items = focusables(*entry.root, viewport_);
  const auto at = std::ranges::find_if(items, [&view](const Focusable &item) {
    return item.widget->id() == view.focused;
  });
  if (at == items.end() && !items.empty()) {
    std::string target;
    if (view.focused.empty()) {
      target = entry.screen->initialFocus();
      if (target.empty() || findIn(*entry.root, target) == nullptr) {
        target = items.front().widget->id();
      }
    } else {
      std::vector<Rect> boxes;
      boxes.reserve(items.size());
      for (const Focusable &item : items) {
        boxes.push_back(item.widget->frame());
      }
      target = items[nearestTo(boxes, view.focusFrame).value_or(0)].widget->id();
    }
    view.focused = target;
    if (!view.focusFromPointer) {
      reveal(entry);
    }
  } else if (items.empty()) {
    view.focused.clear();
  }
  if (view.editing != view.focused) {
    view.editing.clear();
  }
  if (editLater_ && !stack_.empty() && &entry == &stack_.back()) {
    applyEdit(entry, *editLater_);
    editLater_.reset();
  }
  if (Widget *focused = findIn(*entry.root, view.focused)) {
    view.focusFrame = focused->frame();
    std::vector<Widget *> path;
    pathTo(*entry.root, view.focused, path);
    for (Widget *group : path) {
      if (group != focused && group->remembersFocus() && !group->id().empty()) {
        view.remembered[group->id()] = view.focused;
      }
    }
  }
  placeHints();
}

Navigator::Entry *Navigator::fresh() {
  if (stack_.empty()) {
    return nullptr;
  }
  build(stack_.back());
  return &stack_.back();
}

std::vector<Hint> Navigator::hintsOf(const Entry &entry) const {
  std::vector<Hint> shown;
  const Widget *focused = findIn(*entry.root, entry.screen->view().focused);
  if (focused != nullptr) {
    shown = focused->hints();
  }
  for (Hint &hint : entry.screen->hints()) {
    const bool taken = std::ranges::any_of(shown, [&hint](const Hint &have) {
      return !have.keys.empty() && !hint.keys.empty() && have.keys.front() == hint.keys.front();
    });
    if (!taken) {
      shown.push_back(std::move(hint));
    }
  }
  const bool typing = focused != nullptr && focused->takesText();
  std::erase_if(shown, [this, typing](const Hint &hint) {
    return std::ranges::any_of(hint.keys, [this, typing](Action key) {
      return !prompts_.bound(key, typing);
    });
  });
  return shown;
}

void Navigator::placeHints() {
  if (stack_.empty() || stack_.back().root == nullptr) {
    return;
  }
  // A bar shows what the top screen's focus does, though it belongs to a screen under it; when
  // the top screen has a bar of its own, the bars under it, dimmed and partly covered, stay empty.
  const std::vector<Hint> shown = hintsOf(stack_.back());
  std::vector<HintBar *> own;
  findBars(*stack_.back().root, own);
  std::vector<HintBar *> under;
  for (Entry &entry : stack_) {
    if (entry.root != nullptr && &entry != &stack_.back()) {
      findBars(*entry.root, under);
    }
  }
  for (HintBar *bar : own) {
    bar->show(shown, theme_, text_, prompts_);
  }
  for (HintBar *bar : under) {
    bar->show(own.empty() ? shown : std::vector<Hint>{}, theme_, text_, prompts_);
  }
}

void Navigator::layout(const Rect &viewport, float covered) {
  viewport_ = viewport;
  covered_ = covered;
  std::erase_if(leaving_, [this](const Entry &entry) {
    return !entry.presence.moving(now_);
  });
  for (Entry &entry : stack_) {
    build(entry);
  }
  for (Entry &entry : leaving_) {
    build(entry);
  }
  for (Chrome &chrome : chrome_) {
    LayoutContext context{theme_, text_, chrome.screen->view(), now_};
    chrome.root = chrome.screen->build(theme_);
    chrome.root->arrange(context, chrome.screen->area(viewport_, theme_));
  }
  if (stack_.empty()) {
    return;
  }
  ViewState &view = stack_.back().screen->view();
  if (const Widget *focused = findIn(*stack_.back().root, view.focused)) {
    const float ring = theme_.px(theme_.shape.focusGap + theme_.shape.focusRing);
    view.ring.aim(grown(focused->frame(), ring));
    view.ring.step(Timing{now_, theme_.motion.focus});
  }
}

void Navigator::paintEntry(Canvas &canvas, const Entry &entry, bool top) {
  const float presence = std::clamp(entry.presence.value(now_), 0.0f, 1.0f);
  const bool overlay = entry.screen->overlay();
  const ViewState &state = entry.screen->view();
  Painter painter{canvas,        theme_,        text_, prompts_,
                  state.focused, state.hovered, now_,  easeOut(presence),
                  viewport_};
  if (overlay) {
    entry.screen->paintBackdrop(painter, viewport_);
  }
  const float shift = (1.0f - easeOut(presence)) * theme_.px(overlay ? riseDp : slideDp);
  canvas.pushTransform(Transform::translate(overlay ? Point{0.0f, shift} : Point{shift, 0.0f}));
  if (!overlay) {
    entry.screen->paintBackdrop(painter, viewport_);
  }
  entry.root->paint(painter);
  const ViewState &view = entry.screen->view();
  if (top && !entry.leaving) {
    if (const Widget *focused = findIn(*entry.root, view.focused);
        focused != nullptr && !focused->drawsFocus()) {
      const float gap = theme_.px(theme_.shape.focusGap);
      const float width = theme_.px(theme_.shape.focusRing);
      const Rect ring = view.ring.value();
      // The ring stays inside the views that clip the widget.
      std::vector<Widget *> path;
      pathTo(*entry.root, view.focused, path);
      Rect inside = viewport_;
      for (const Widget *above : path) {
        if (const std::optional<Rect> clip = above->clip(); clip && above != focused) {
          inside = intersection(inside, grown(*clip, gap + width));
        }
      }
      canvas.pushClip(inside);
      canvas.strokeRoundRect(ring, focused->cornerRadius(theme_) + gap + width, width,
                             painter.fade(theme_.colours.focusRing));
      canvas.popClip();
    }
  }
  canvas.popTransform();
}

std::size_t Navigator::firstDrawn() const {
  std::size_t first = stack_.empty() ? 0 : stack_.size() - 1;
  while (first > 0 && (stack_[first].screen->overlay() || stack_[first].presence.moving(now_))) {
    --first;
  }
  return first;
}

void Navigator::paintLayer(Canvas &canvas, bool overlays) {
  // The overlays are the screens above the highest one that is not.
  std::size_t pages = 0;
  for (std::size_t i = 0; i < stack_.size(); ++i) {
    if (!stack_[i].screen->overlay()) {
      pages = i + 1;
    }
  }
  for (std::size_t i = firstDrawn(); i < stack_.size(); ++i) {
    if ((i >= pages) == overlays) {
      paintEntry(canvas, stack_[i], i + 1 == stack_.size());
    }
  }
  for (const Entry &entry : leaving_) {
    if (entry.screen->overlay() == overlays) {
      paintEntry(canvas, entry, false);
    }
  }
}

void Navigator::paint(Canvas &canvas) {
  paintPages(canvas);
  paintOverlays(canvas);
}

void Navigator::paintPages(Canvas &canvas) {
  paintLayer(canvas, false);
}

void Navigator::paintOverlays(Canvas &canvas) {
  paintLayer(canvas, true);
}

void Navigator::play(const std::function<void()> &sound) const {
  if (sound) {
    sound();
  }
}

void Navigator::focusOn(Entry &entry, const std::string &id, bool pointer) {
  ViewState &view = entry.screen->view();
  if (view.focused == id) {
    return;
  }
  Widget *widget = findIn(*entry.root, id);
  if (widget == nullptr || !widget->focusable()) {
    return;
  }
  view.focused = id;
  view.focusFromPointer = pointer;
  view.focusFrame = widget->frame();
  play(sounds_.move);
  ++busy_;
  Context context{*this, *entry.screen};
  widget->focused(context);
  --busy_;
  build(entry);
  if (!pointer) {
    reveal(entry);
  }
  settle(context);
}

void Navigator::reveal(Entry &entry) {
  ViewState &view = entry.screen->view();
  std::vector<Widget *> path;
  if (!pathTo(*entry.root, view.focused, path)) {
    return;
  }
  const Widget &focused = *path.back();
  const float margin = theme_.px(theme_.spacing.section);
  for (Widget *above : path) {
    auto *scroll = dynamic_cast<ScrollView *>(above);
    if (scroll == nullptr || scroll->children().empty()) {
      continue;
    }
    ViewState::Scroll &state = view.scroll(scroll->id());
    const Widget &content = *scroll->children().front();
    const std::vector<Focusable> inside = focusables(*scroll, viewport_);
    const bool first = !inside.empty() && inside.front().widget == &focused;
    const bool last = !inside.empty() && inside.back().widget == &focused;
    const float top = first ? 0.0f : focused.frame().y - content.frame().y - margin;
    const float bottom =
        last ? state.limit + state.viewport : focused.frame().bottom() - content.frame().y + margin;
    float target = state.offset.target();
    if (bottom > target + state.viewport) {
      target = bottom - state.viewport;
    }
    if (top < target) {
      target = top;
    }
    state.offset.aim(std::clamp(target, 0.0f, state.limit));
  }
}

bool Navigator::nudge(Entry &entry, int rows) {
  ViewState &view = entry.screen->view();
  ScrollView *scroll = nullptr;
  std::vector<Widget *> path;
  if (pathTo(*entry.root, view.focused, path)) {
    for (auto above = path.rbegin(); above != path.rend() && scroll == nullptr; ++above) {
      scroll = dynamic_cast<ScrollView *>(*above);
    }
  } else {
    scroll = firstScroll(*entry.root, view);
  }
  if (scroll == nullptr) {
    return false;
  }
  ViewState::Scroll &state = view.scroll(scroll->id());
  const float from = state.offset.target();
  const float to = std::clamp(from + static_cast<float>(rows) * theme_.px(theme_.spacing.rowHeight),
                              0.0f, state.limit);
  if (to == from) {
    return false;
  }
  state.offset.aim(to);
  return true;
}

bool Navigator::move(Entry &entry, Direction direction) {
  ViewState &view = entry.screen->view();
  const std::vector<Focusable> items = focusables(*entry.root, viewport_);
  const auto here = std::ranges::find_if(items, [&view](const Focusable &item) {
    return item.widget->id() == view.focused;
  });
  if (here == items.end()) {
    return false;
  }
  std::vector<Rect> boxes;
  std::vector<std::size_t> indexes;
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (items[i].widget != here->widget) {
      boxes.push_back(items[i].widget->frame());
      indexes.push_back(i);
    }
  }
  const std::optional<std::size_t> picked =
      nearestInDirection(boxes, here->widget->frame(), direction);
  if (!picked) {
    return false;
  }
  const Focusable &candidate = items[indexes[*picked]];
  std::string target = candidate.widget->id();
  // Entering a group from outside lands where it prefers, else where it last was, else on its
  // first child in sight.
  for (Widget *group : candidate.groups) {
    if (std::ranges::find(here->groups, group) != here->groups.end()) {
      continue;
    }
    std::optional<std::string> landing = group->preferredFocus();
    if (!landing) {
      if (const auto kept = view.remembered.find(group->id()); kept != view.remembered.end()) {
        landing = kept->second;
      }
    }
    if (landing && findIn(*group, *landing) != nullptr) {
      target = *landing;
    } else if (const auto first = std::ranges::find_if(
                   items,
                   [group](const Focusable &item) {
                     return item.visible.height > 0.0f &&
                            std::ranges::find(item.groups, group) != item.groups.end();
                   });
               first != items.end()) {
      target = first->widget->id();
    }
    break;
  }
  focusOn(entry, target, false);
  return true;
}

void Navigator::settle(Context &context) {
  if (busy_ > 0) {
    return;
  }
  if (focusLater_ && !context.focusTo) {
    context.focusTo = std::move(focusLater_);
  }
  focusLater_.reset();
  if (context.focusTo && !stack_.empty()) {
    Entry &entry = *fresh();
    focusOn(entry, *context.focusTo, false);
    context.focusTo.reset();
  }
  dropClosing();
  std::vector<Deferred> deferred = std::move(deferred_);
  deferred_.clear();
  for (Deferred &op : deferred) {
    switch (op.kind) {
    case Deferred::Kind::Push:
      push(std::move(op.screen));
      break;
    case Deferred::Kind::Pop:
      pop();
      break;
    case Deferred::Kind::Replace:
      replace(std::move(op.screen));
      break;
    case Deferred::Kind::Clear:
      clear();
      break;
    }
  }
  const std::vector<Action> performed = std::move(context.performed);
  context.performed.clear();
  for (const Action action : performed) {
    act(action);
  }
  fresh();
}

bool Navigator::act(Action action) {
  Entry *entry = fresh();
  if (entry == nullptr) {
    return false;
  }
  Screen &screen = *entry->screen;
  Context context{*this, screen};
  ++busy_;
  Widget *focused = findIn(*entry->root, screen.view().focused);
  bool handled = focused != nullptr && focused->act(action, context);
  if (handled && action == Action::Accept) {
    play(sounds_.activate);
  }
  if (!handled) {
    handled = screen.act(action, context);
  }
  --busy_;
  if (!handled) {
    switch (action) {
    case Action::Up:
      handled = move(*entry, Direction::Up) || nudge(*entry, -1);
      break;
    case Action::Down:
      handled = move(*entry, Direction::Down) || nudge(*entry, 1);
      break;
    case Action::Left:
      handled = move(*entry, Direction::Left);
      break;
    case Action::Right:
      handled = move(*entry, Direction::Right);
      break;
    case Action::PreviousTab:
    case Action::NextTab:
      // A dock's tabs are chosen by the app's hook, which sounds them.
      if (TabDock *dock = findTabs(*entry->root) == nullptr ? dockFor(*entry) : nullptr) {
        ++busy_;
        dock->step(action == Action::NextTab ? 1 : -1);
        --busy_;
        handled = true;
      } else if (Tabs *tabs = findTabs(*entry->root)) {
        const bool onTab = focused != nullptr && focused->id().starts_with(tabs->id() + "/");
        if (const std::optional<std::string> chosen =
                tabs->step(action == Action::NextTab ? 1 : -1)) {
          handled = true;
          if (onTab) {
            context.focus(*chosen);
          } else {
            play(sounds_.move);
          }
        }
      }
      break;
    case Action::Back: {
      ++busy_;
      const Screen::BackReply reply = screen.back(context);
      --busy_;
      // The root stays whatever its screen answers.
      if (reply == Screen::BackReply::Pop && !(root_ && entry == &stack_.front())) {
        screen.close();
        play(sounds_.back);
      }
      handled = true;
      break;
    }
    default:
      break;
    }
  }
  settle(context);
  return handled;
}

TabDock *Navigator::dockFor(Entry &entry) {
  if (TabDock *dock = findDock(*entry.root)) {
    return dock;
  }
  for (Chrome &chrome : chrome_) {
    if (chrome.root == nullptr) {
      continue;
    }
    if (TabDock *dock = findDock(*chrome.root)) {
      return dock;
    }
  }
  return nullptr;
}

bool Navigator::pointChrome(const PointerEvent &event) {
  for (auto chrome = chrome_.rbegin(); chrome != chrome_.rend(); ++chrome) {
    if (chrome->root == nullptr) {
      continue;
    }
    Widget *hit = hitTest(*chrome->root, event.at);
    if (hit == nullptr) {
      continue;
    }
    ViewState &view = chrome->screen->view();
    Context context{*this, *chrome->screen};
    // The chrome is no screen of the stack, so what its widgets do to the stack happens at once.
    switch (event.kind) {
    case PointerKind::Move:
      view.hovered = hit->id();
      hit->hover(event.at, context);
      break;
    case PointerKind::Press:
      hit->press(event.at, context);
      break;
    case PointerKind::Context:
      hit->context(event.at, context);
      break;
    default:
      break;
    }
    settle(context);
    return true;
  }
  return false;
}

bool Navigator::moveOn(Entry &entry, Point at) {
  Widget *hit = hitTest(*entry.root, at);
  entry.screen->view().hovered = hit != nullptr ? hit->id() : std::string{};
  if (hit == nullptr) {
    return false;
  }
  if (hit->focusable()) {
    focusOn(entry, hit->id(), true);
    hit = hitTest(*fresh()->root, at);
    if (hit == nullptr) {
      return true;
    }
  }
  Context context{*this, *stack_.back().screen};
  ++busy_;
  hit->hover(at, context);
  --busy_;
  settle(context);
  return true;
}

bool Navigator::pointer(const PointerEvent &event) {
  const bool overChrome =
      event.kind != PointerKind::Leave && event.kind != PointerKind::Wheel && pointChrome(event);
  if (event.kind == PointerKind::Move || event.kind == PointerKind::Leave || overChrome) {
    for (Chrome &chrome : chrome_) {
      if (!overChrome || chrome.root == nullptr || hitTest(*chrome.root, event.at) == nullptr) {
        chrome.screen->view().hovered.clear();
      }
    }
  }
  Entry *entry = fresh();
  if (entry == nullptr) {
    return overChrome;
  }
  if (overChrome || event.kind == PointerKind::Leave) {
    entry->screen->view().hovered.clear();
    return overChrome;
  }
  Screen &screen = *entry->screen;
  ViewState &view = screen.view();
  switch (event.kind) {
  case PointerKind::Move:
    return moveOn(*entry, event.at);
  case PointerKind::Press: {
    Widget *hit = hitTest(*entry->root, event.at);
    if (hit == nullptr) {
      // A press beside an overlay's card closes it, as Back would.
      const auto children = entry->root->children();
      if (screen.overlay() &&
          (children.empty() || !contains(children.front()->frame(), event.at))) {
        return act(Action::Back);
      }
      return false;
    }
    if (hit->focusable()) {
      focusOn(*entry, hit->id(), true);
      entry = fresh();
      hit = hitTest(*entry->root, event.at);
      if (hit == nullptr) {
        return true;
      }
    }
    Context context{*this, *entry->screen};
    ++busy_;
    const bool handled = hit->press(event.at, context);
    --busy_;
    if (handled && hit->focusable()) {
      play(sounds_.activate);
    }
    settle(context);
    return true;
  }
  case PointerKind::Context: {
    moveOn(*entry, event.at);
    entry = fresh();
    Widget *hit = hitTest(*entry->root, event.at);
    bool handled = false;
    if (hit != nullptr) {
      Context context{*this, *entry->screen};
      ++busy_;
      handled = hit->context(event.at, context);
      --busy_;
      settle(context);
    }
    // Else it is the second action of what is focused now.
    return handled || act(Action::Secondary);
  }
  case PointerKind::Wheel: {
    if (ScrollView *scroll = scrollAt(*entry->root, event.at, view)) {
      ViewState::Scroll &state = view.scroll(scroll->id());
      const float step = theme_.px(theme_.spacing.rowHeight) * wheelRows;
      state.offset.aim(std::clamp(state.offset.target() - event.wheel * step, 0.0f, state.limit));
      view.focusFromPointer = true;
      return true;
    }
    Widget *focused = findIn(*entry->root, view.focused);
    if (focused == nullptr) {
      return false;
    }
    Context context{*this, screen};
    ++busy_;
    const bool handled = focused->wheel(event.wheel, context);
    --busy_;
    settle(context);
    return handled;
  }
  case PointerKind::Leave:
    break;
  }
  return false;
}

bool Navigator::text(std::string_view typed) {
  Entry *entry = fresh();
  if (entry == nullptr) {
    return false;
  }
  Screen &screen = *entry->screen;
  Context context{*this, screen};
  ++busy_;
  Widget *focused = findIn(*entry->root, screen.view().focused);
  const bool handled =
      (focused != nullptr && focused->type(typed, context)) || screen.text(typed, context);
  --busy_;
  settle(context);
  return handled;
}

bool Navigator::typing() const {
  if (stack_.empty() || !stack_.back().root) {
    return false;
  }
  const Entry &entry = stack_.back();
  const Widget *focused = findIn(*entry.root, entry.screen->view().focused);
  return focused != nullptr && focused->takesText();
}

bool Navigator::editing() const {
  if (!typing()) {
    return false;
  }
  const ViewState &view = stack_.back().screen->view();
  return view.editing == view.focused;
}

void Navigator::edit(bool on) {
  if (busy_ > 0 || stack_.empty() || !stack_.back().root) {
    editLater_ = on;
    return;
  }
  applyEdit(stack_.back(), on);
}

void Navigator::applyEdit(Entry &entry, bool on) {
  ViewState &view = entry.screen->view();
  if (!on) {
    view.editing.clear();
    return;
  }
  const Widget *focused = findIn(*entry.root, view.focused);
  if (focused != nullptr && focused->takesText() && view.editing != view.focused) {
    view.editing = view.focused;
    view.caret = SIZE_MAX;
  }
}

std::vector<Hint> Navigator::hints() {
  const Entry *entry = fresh();
  return entry == nullptr ? std::vector<Hint>{} : hintsOf(*entry);
}

std::string Navigator::focused() const {
  return stack_.empty() ? std::string{} : stack_.back().screen->view().focused;
}

bool Navigator::focus(std::string_view id) {
  if (busy_ > 0) {
    focusLater_ = std::string{id};
    return true;
  }
  Entry *entry = fresh();
  if (entry == nullptr) {
    return false;
  }
  Widget *widget = findIn(*entry->root, id);
  if (widget == nullptr || !widget->focusable()) {
    return false;
  }
  focusOn(*entry, std::string{id}, false);
  return true;
}

const Widget *Navigator::find(std::string_view id) {
  Entry *entry = fresh();
  return entry == nullptr ? nullptr : findIn(*entry->root, id);
}

} // namespace gpu2d::ui
