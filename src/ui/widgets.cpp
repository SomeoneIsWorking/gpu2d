#include "gpu2d/ui/widgets.hpp"

#include <algorithm>
#include <cmath>
#include <memory>

#include "gpu2d/ui/screen.hpp"
#include "gpu2d/ui/text_fit.hpp"

namespace gpu2d::ui {
namespace {

/// A switch's track, in dp.
constexpr float switchWidthDp = 52.0f;
constexpr float switchHeightDp = 30.0f;
/// A slider's track and the number after it, in dp.
constexpr float trackWidthDp = 200.0f;
constexpr float trackHeightDp = 6.0f;
constexpr float knobDp = 22.0f;
constexpr float numberWidthDp = 56.0f;
/// A chevron's half height and its stroke, in dp.
constexpr float chevronDp = 6.0f;
constexpr float strokeDp = 2.2f;
/// The share of a row its end may take.
constexpr float trailingShare = 0.45f;
/// A button's height, in dp.
constexpr float buttonHeightDp = 52.0f;
/// A chip's height and its widest, in dp.
constexpr float chipHeightDp = 26.0f;
constexpr float chipMaxWidthDp = 220.0f;
/// Glyphs in the hint bar against the hint text size.
constexpr float glyphShare = 1.55f;
/// A chevron pointing `sign` (1 right, -1 left) with its tip at `tipX`.
void chevron(const Painter &painter, float tipX, float centreY, float sign, Colour colour) {
  const float half = painter.px(chevronDp);
  const float stroke = painter.px(strokeDp);
  const Colour ink = painter.fade(colour);
  const Point tip{tipX, centreY};
  const Point top{tipX - sign * half, centreY - half};
  const Point bottom{tipX - sign * half, centreY + half};
  painter.canvas.line(top, tip, stroke, ink);
  painter.canvas.line(tip, bottom, stroke, ink);
  painter.canvas.fillCircle(tip, stroke * 0.5f, ink);
  painter.canvas.fillCircle(top, stroke * 0.5f, ink);
  painter.canvas.fillCircle(bottom, stroke * 0.5f, ink);
}

/// A tick inside `box`.
void tick(const Painter &painter, const Rect &box, Colour colour) {
  const float stroke = painter.px(strokeDp) * 1.2f;
  const Colour ink = painter.fade(colour);
  const Point a{box.x + box.width * 0.15f, box.y + box.height * 0.55f};
  const Point b{box.x + box.width * 0.42f, box.y + box.height * 0.8f};
  const Point c{box.x + box.width * 0.88f, box.y + box.height * 0.22f};
  painter.canvas.line(a, b, stroke, ink);
  painter.canvas.line(b, c, stroke, ink);
  for (const Point p : {a, b, c}) {
    painter.canvas.fillCircle(p, stroke * 0.5f, ink);
  }
}

float rowRadius(const Theme &theme) {
  return theme.px(theme.shape.radius);
}

} // namespace

// Label

float Label::sizeDp(const Theme &theme) const noexcept {
  switch (role_) {
  case TextRole::Title:
    return theme.type.title;
  case TextRole::Heading:
    return theme.type.heading;
  case TextRole::Caption:
    return theme.type.caption;
  case TextRole::Body:
    break;
  }
  return theme.type.body;
}

Colour Label::ink(const Theme &theme) const noexcept {
  if (colour_) {
    return *colour_;
  }
  switch (role_) {
  case TextRole::Heading:
    return theme.colours.inkFaint;
  case TextRole::Caption:
    return theme.colours.inkMuted;
  case TextRole::Title:
  case TextRole::Body:
    break;
  }
  return theme.colours.ink;
}

Size Label::measure(LayoutContext &context, float maxWidth) {
  const float size = context.theme.px(sizeDp(context.theme));
  const float line = context.text.lineHeight(size);
  if (!wraps_) {
    return Size{std::min(maxWidth, context.text.measure(text_, size)), line};
  }
  const std::vector<std::string> lines = wrapped(context.text, text_, maxWidth, size);
  float widest = 0.0f;
  for (const std::string &each : lines) {
    widest = std::max(widest, context.text.measure(each, size));
  }
  return Size{widest, line * static_cast<float>(std::max<std::size_t>(lines.size(), 1))};
}

void Label::arrange(LayoutContext &context, const Rect &frame) {
  frame_ = frame;
  const float size = context.theme.px(sizeDp(context.theme));
  lines_ = wraps_ ? wrapped(context.text, text_, frame.width, size)
                  : std::vector<std::string>{fitted(context.text, text_, frame.width, size)};
}

void Label::paint(Painter &painter) const {
  const float size = painter.px(sizeDp(painter.theme));
  const float line = painter.text.lineHeight(size);
  float y = frame_.y;
  for (const std::string &each : lines_) {
    painter.label(each, Point{frame_.x, y + line * 0.5f}, sizeDp(painter.theme),
                  ink(painter.theme));
    y += line;
  }
}

// SectionHeading

Size SectionHeading::measure(LayoutContext &context, float maxWidth) {
  const Theme &theme = context.theme;
  return Size{maxWidth, theme.px(theme.spacing.section) +
                            context.text.lineHeight(theme.px(theme.type.heading)) +
                            theme.px(theme.spacing.gap)};
}

void SectionHeading::paint(Painter &painter) const {
  const Theme &theme = painter.theme;
  const float line = painter.text.lineHeight(painter.px(theme.type.heading));
  const float centreY = frame_.bottom() - painter.px(theme.spacing.gap) - line * 0.5f;
  painter.label(text_, Point{frame_.x + painter.px(theme.spacing.pad * 0.5f), centreY},
                theme.type.heading, theme.colours.inkFaint);
}

// Button

Size Button::measure(LayoutContext &context, float maxWidth) {
  const Theme &theme = context.theme;
  const float text = context.text.measure(label_, theme.px(theme.type.body));
  const float width =
      fixedWidth() ? theme.px(*fixedWidth()) : text + theme.px(theme.spacing.pad) * 3.0f;
  return Size{std::min(maxWidth, width), theme.px(buttonHeightDp)};
}

float Button::cornerRadius(const Theme & /*theme*/) const {
  return frame_.height * 0.5f;
}

bool Button::act(Action action, ActContext & /*context*/) {
  if (action != Action::Accept) {
    return false;
  }
  if (pressed_) {
    pressed_();
  }
  return true;
}

std::vector<Hint> Button::hints() const {
  return {Hint{{Action::Accept}, label_}};
}

void Button::paint(Painter &painter) const {
  const Theme &theme = painter.theme;
  const bool focused = painter.focused == id() && !id().empty();
  Colour fill = focused ? theme.colours.controlFocused : theme.colours.control;
  Colour ink = theme.colours.ink;
  if (kind_ == ButtonKind::Primary) {
    fill = theme.colours.accent;
    ink = theme.colours.onAccent;
  } else if (kind_ == ButtonKind::Danger) {
    fill = theme.colours.danger;
    ink = theme.colours.onAccent;
  }
  painter.canvas.fillRoundRect(frame_, frame_.height * 0.5f, painter.fade(fill));
  if (kind_ == ButtonKind::Normal) {
    painter.canvas.strokeRoundRect(frame_, frame_.height * 0.5f, painter.px(theme.shape.outline),
                                   painter.fade(theme.colours.track));
  }
  const float size = painter.px(theme.type.body);
  const std::string shown =
      fitted(painter.text, label_, frame_.width - painter.px(theme.spacing.pad), size);
  const float width = painter.text.measure(shown, size);
  painter.label(shown,
                Point{frame_.x + (frame_.width - width) * 0.5f, frame_.y + frame_.height * 0.5f},
                theme.type.body, ink);
}

// Item

Size Item::measure(LayoutContext &context, float maxWidth) {
  const Theme &theme = context.theme;
  const float height =
      text_.subtitle.empty() ? theme.spacing.compactRowHeight : theme.spacing.rowHeight;
  return Size{maxWidth, theme.px(fixedHeight().value_or(height))};
}

float Item::trailingWidth(LayoutContext &context, float width) {
  const Theme &theme = context.theme;
  float wanted = 0.0f;
  if (marked_) {
    wanted = theme.px(theme.type.value) * 1.2f;
  } else if (!value_.empty()) {
    wanted = context.text.measure(value_, theme.px(theme.type.value));
  }
  if (opens_) {
    wanted += theme.px(chevronDp * 2.0f + theme.spacing.gap * 2.0f);
  }
  return std::min(wanted, width * trailingShare);
}

void Item::arrange(LayoutContext &context, const Rect &frame) {
  frame_ = frame;
  const Theme &theme = context.theme;
  const float pad = theme.px(theme.spacing.pad);
  const float end = trailingWidth(context, frame.width - 2.0f * pad);
  trailing_ = Rect{frame.right() - pad - end, frame.y, end, frame.height};
  float room = std::max(0.0f, frame.width - 2.0f * pad - end -
                                  (end > 0.0f ? theme.px(theme.spacing.pad) : 0.0f));
  chipBox_ = Rect{};
  chipShown_.clear();
  if (!chip_.empty()) {
    const float size = theme.px(theme.type.caption);
    const float inset = theme.px(theme.spacing.gap * 1.5f);
    const float widest = std::min(theme.px(chipMaxWidthDp), room * 0.5f);
    chipShown_ = fitted(context.text, chip_, std::max(0.0f, widest - 2.0f * inset), size);
    const float width = context.text.measure(chipShown_, size) + 2.0f * inset;
    const float height = theme.px(chipHeightDp);
    const float right =
        end > 0.0f ? trailing_.x - theme.px(theme.spacing.gap * 2.0f) : frame.right() - pad;
    chipBox_ = Rect{right - width, frame.y + (frame.height - height) * 0.5f, width, height};
    room = std::max(0.0f, room - width - theme.px(theme.spacing.gap * 2.0f));
  }
  titleShown_ = fitted(context.text, text_.title, room, theme.px(theme.type.body));
  subtitleShown_ = fitted(context.text, text_.subtitle, room, theme.px(theme.type.caption));
  const float chevronRoom = opens_ ? theme.px(chevronDp * 2.0f + theme.spacing.gap * 2.0f) : 0.0f;
  valueShown_ =
      fitted(context.text, value_, std::max(0.0f, end - chevronRoom), theme.px(theme.type.value));
}

bool Item::focusable() const {
  return !disabled_ && static_cast<bool>(pressed_);
}

float Item::cornerRadius(const Theme &theme) const {
  return rowRadius(theme);
}

bool Item::act(Action action, ActContext &context) {
  if (action != Action::Accept || !pressed_ || disabled_) {
    return actExtra(action);
  }
  if (confirm_) {
    context.push(confirm(confirm_->question, confirm_->detail, confirm_->verb, pressed_));
    return true;
  }
  pressed_();
  return true;
}

bool Item::actExtra(Action action) {
  if (disabled_) {
    return false;
  }
  const auto extra = std::ranges::find(extras_, action, &Extra::key);
  if (extra == extras_.end() || !extra->action) {
    return false;
  }
  extra->action();
  return true;
}

void Item::addExtraHints(std::vector<Hint> &hints) const {
  if (disabled_) {
    return;
  }
  for (const Extra &extra : extras_) {
    hints.push_back(Hint{{extra.key}, extra.verb});
  }
}

std::vector<Hint> Item::hints() const {
  if (!pressed_ || disabled_) {
    return {};
  }
  std::vector<Hint> hints{Hint{{Action::Accept}, verb_}};
  addExtraHints(hints);
  return hints;
}

void Item::paintTrailing(Painter &painter, const Rect &area) const {
  const Theme &theme = painter.theme;
  const float centreY = area.y + area.height * 0.5f;
  float right = area.right();
  if (opens_) {
    chevron(painter, right - painter.px(chevronDp * 0.5f), centreY, 1.0f, theme.colours.inkMuted);
    right -= painter.px(chevronDp * 2.0f + theme.spacing.gap * 2.0f);
  }
  if (marked_) {
    const float box = painter.px(theme.type.value);
    tick(painter, Rect{right - box, centreY - box * 0.5f, box, box}, theme.colours.accent);
    return;
  }
  painter.labelEndingAt(valueShown_, Point{right, centreY}, theme.type.value,
                        theme.colours.inkMuted);
}

void Item::paint(Painter &painter) const {
  const Theme &theme = painter.theme;
  const bool focused = painter.focused == id() && !id().empty();
  const float alpha = painter.alpha;
  if (disabled_) {
    painter.alpha *= 0.5f;
  }
  painter.canvas.fillRoundRect(
      frame_, rowRadius(theme),
      painter.fade(focused ? theme.colours.controlFocused : theme.colours.control));
  const float x = frame_.x + painter.px(theme.spacing.pad);
  const float centreY = frame_.y + frame_.height * 0.5f;
  if (subtitleShown_.empty()) {
    painter.label(titleShown_, Point{x, centreY}, theme.type.body, theme.colours.ink);
  } else {
    const float title = painter.text.lineHeight(painter.px(theme.type.body));
    const float subtitle = painter.text.lineHeight(painter.px(theme.type.caption));
    const float gap = painter.px(2.0f);
    const float top = centreY - (title + gap + subtitle) * 0.5f;
    painter.label(titleShown_, Point{x, top + title * 0.5f}, theme.type.body, theme.colours.ink);
    painter.label(subtitleShown_, Point{x, top + title + gap + subtitle * 0.5f}, theme.type.caption,
                  theme.colours.inkMuted);
  }
  if (!chipShown_.empty()) {
    const float radius = chipBox_.height * 0.5f;
    if (chipStrong_) {
      painter.canvas.fillRoundRect(chipBox_, radius, painter.fade(theme.colours.accent));
    } else {
      painter.canvas.strokeRoundRect(chipBox_, radius, painter.px(theme.shape.outline),
                                     painter.fade(theme.colours.inkFaint));
    }
    const float width = painter.text.measure(chipShown_, painter.px(theme.type.caption));
    painter.label(
        chipShown_,
        Point{chipBox_.x + (chipBox_.width - width) * 0.5f, chipBox_.y + chipBox_.height * 0.5f},
        theme.type.caption, chipStrong_ ? theme.colours.onAccent : theme.colours.inkMuted);
  }
  if (trailing_.width > 0.0f) {
    paintTrailing(painter, trailing_);
  }
  painter.alpha = alpha;
}

// Toggle

float Toggle::trailingWidth(LayoutContext &context, float /*width*/) {
  return context.theme.px(switchWidthDp);
}

bool Toggle::act(Action action, ActContext & /*context*/) {
  if (action != Action::Accept || !interactive()) {
    return actExtra(action);
  }
  on_ = !on_;
  if (changed_) {
    changed_(on_);
  }
  return true;
}

std::vector<Hint> Toggle::hints() const {
  std::vector<Hint> hints{Hint{{Action::Accept}, on_ ? "Turn off" : "Turn on"}};
  addExtraHints(hints);
  return hints;
}

void Toggle::paintTrailing(Painter &painter, const Rect &area) const {
  const Theme &theme = painter.theme;
  const float height = painter.px(switchHeightDp);
  const Rect track{area.right() - painter.px(switchWidthDp), area.y + (area.height - height) * 0.5f,
                   painter.px(switchWidthDp), height};
  painter.canvas.fillRoundRect(track, height * 0.5f,
                               painter.fade(on_ ? theme.colours.accent : theme.colours.track));
  const float inset = painter.px(3.0f);
  const float knob = height * 0.5f - inset;
  const float centreX = on_ ? track.right() - inset - knob : track.x + inset + knob;
  painter.canvas.fillCircle(Point{centreX, track.y + height * 0.5f}, knob,
                            painter.fade(theme.colours.onAccent));
}

// Choice

float Choice::trailingWidth(LayoutContext &context, float width) {
  const Theme &theme = context.theme;
  float widest = 0.0f;
  for (const std::string &option : options_) {
    widest = std::max(widest, context.text.measure(option, theme.px(theme.type.value)));
  }
  const float chevrons = theme.px(chevronDp * 2.0f + theme.spacing.gap * 2.0f) * 2.0f;
  const float end = std::min(widest + chevrons, width * trailingShare);
  shown_ = chosen_ < options_.size()
               ? fitted(context.text, options_[chosen_], end - chevrons, theme.px(theme.type.value))
               : std::string{};
  return end;
}

void Choice::stepBy(int step, ActContext &context) {
  if (options_.empty()) {
    return;
  }
  const auto count = static_cast<long>(options_.size());
  const long next = ((static_cast<long>(chosen_) + step) % count + count) % count;
  chosen_ = static_cast<std::size_t>(next);
  context.sound(Sound::Move);
  if (changed_) {
    changed_(chosen_);
  }
}

void Choice::openList(ActContext &context) {
  context.push(std::make_unique<ChoiceList>(
      text().title, listed_.size() == options_.size() ? listed_ : options_, chosen_, changed_));
}

bool Choice::act(Action action, ActContext &context) {
  if (!focusable()) {
    return false;
  }
  switch (action) {
  case Action::Left:
    stepBy(-1, context);
    return true;
  case Action::Right:
    stepBy(1, context);
    return true;
  case Action::Accept:
    openList(context);
    return true;
  default:
    return actExtra(action);
  }
}

bool Choice::press(Point at, ActContext &context) {
  if (!focusable()) {
    return false;
  }
  const float zone = context.theme().px(chevronDp * 2.0f + context.theme().spacing.gap * 2.0f);
  if (at.x >= trailing().x && at.x < trailing().x + zone) {
    stepBy(-1, context);
  } else if (at.x >= trailing().right() - zone && at.x < trailing().right() + zone) {
    stepBy(1, context);
  } else {
    openList(context);
  }
  return true;
}

std::vector<Hint> Choice::hints() const {
  std::vector<Hint> hints{Hint{{Action::Left, Action::Right}, "Change"},
                          Hint{{Action::Accept}, "Choose"}};
  addExtraHints(hints);
  return hints;
}

void Choice::paintTrailing(Painter &painter, const Rect &area) const {
  const Theme &theme = painter.theme;
  const float centreY = area.y + area.height * 0.5f;
  const float zone = painter.px(chevronDp * 2.0f + theme.spacing.gap * 2.0f);
  const bool focused = painter.focused == id();
  const Colour arrows = focused ? theme.colours.ink : theme.colours.inkFaint;
  chevron(painter, area.x + painter.px(chevronDp * 0.5f), centreY, -1.0f, arrows);
  chevron(painter, area.right() - painter.px(chevronDp * 0.5f), centreY, 1.0f, arrows);
  const float size = painter.px(theme.type.value);
  const float width = painter.text.measure(shown_, size);
  const float middle = area.x + zone + (area.width - 2.0f * zone - width) * 0.5f;
  painter.label(shown_, Point{middle, centreY}, theme.type.value,
                focused ? theme.colours.ink : theme.colours.inkMuted);
}

// Slider

float Slider::trailingWidth(LayoutContext &context, float width) {
  // The title keeps the room it needs; the track takes the rest, up to its full length.
  const Theme &theme = context.theme;
  const float title = context.text.measure(text().title, theme.px(theme.type.body));
  const float room = std::max(width * trailingShare, width - title - theme.px(theme.spacing.pad));
  return std::min(theme.px(trackWidthDp + numberWidthDp), room);
}

Rect Slider::track(const Theme &theme) const noexcept {
  const Rect &area = trailing();
  const float knob = theme.px(knobDp) * 0.5f;
  const float width = std::max(0.0f, area.width - theme.px(numberWidthDp) - 2.0f * knob);
  return Rect{area.x + knob, area.y + (area.height - theme.px(trackHeightDp)) * 0.5f, width,
              theme.px(trackHeightDp)};
}

void Slider::set(int level, ActContext &context) {
  const int next = std::clamp(level, range_.low, range_.high);
  if (next == level_) {
    return;
  }
  level_ = next;
  context.sound(Sound::Move);
  if (changed_) {
    changed_(level_);
  }
}

bool Slider::act(Action action, ActContext &context) {
  if (!interactive()) {
    return false;
  }
  if (action == Action::Left || action == Action::Right) {
    set(level_ + (action == Action::Left ? -range_.step : range_.step), context);
    return true;
  }
  return actExtra(action);
}

bool Slider::press(Point at, ActContext &context) {
  const Rect line = track(context.theme());
  const float slack = context.theme().px(knobDp);
  if (!interactive() || line.width <= 0.0f || at.x < line.x - slack ||
      at.x > line.right() + slack) {
    return false;
  }
  const float t = std::clamp((at.x - line.x) / line.width, 0.0f, 1.0f);
  const float span = static_cast<float>(range_.high - range_.low);
  const int steps = std::max(range_.step, 1);
  const int raw = range_.low + static_cast<int>(std::lround(t * span));
  set(range_.low + (raw - range_.low + steps / 2) / steps * steps, context);
  return true;
}

std::vector<Hint> Slider::hints() const {
  std::vector<Hint> hints{Hint{{Action::Left, Action::Right}, "Adjust"}};
  addExtraHints(hints);
  return hints;
}

void Slider::paintTrailing(Painter &painter, const Rect &area) const {
  const Theme &theme = painter.theme;
  const Rect line = track(theme);
  const float span = static_cast<float>(std::max(range_.high - range_.low, 1));
  const float t = static_cast<float>(level_ - range_.low) / span;
  painter.canvas.fillRoundRect(line, line.height * 0.5f, painter.fade(theme.colours.track));
  painter.canvas.fillRoundRect(Rect{line.x, line.y, line.width * t, line.height},
                               line.height * 0.5f, painter.fade(theme.colours.accent));
  const Point knob{line.x + line.width * t, line.y + line.height * 0.5f};
  const float radius = painter.px(knobDp) * 0.5f;
  painter.canvas.fillCircle(knob, radius, painter.fade(theme.colours.accent));
  painter.canvas.fillCircle(knob, radius - painter.px(3.0f), painter.fade(theme.colours.onAccent));
  painter.labelEndingAt(std::to_string(level_) + unit_,
                        Point{area.right(), area.y + area.height * 0.5f}, theme.type.value,
                        theme.colours.ink);
}

// Progress

Size Progress::measure(LayoutContext &context, float maxWidth) {
  return Size{maxWidth, context.theme.px(fixedHeight().value_or(trackHeightDp))};
}

void Progress::paint(Painter &painter) const {
  const Theme &theme = painter.theme;
  const float radius = frame_.height * 0.5f;
  painter.canvas.fillRoundRect(frame_, radius, painter.fade(theme.colours.track));
  const float filled = std::clamp(fraction_, 0.0f, 1.0f);
  if (filled > 0.0f) {
    painter.canvas.fillRoundRect(
        Rect{frame_.x, frame_.y, std::max(frame_.height, frame_.width * filled), frame_.height},
        radius, painter.fade(theme.colours.accent));
  }
}

// Tabs

namespace {

/// One tab: focusing it chooses it; A goes on into what it shows.
class TabButton : public Widget {
public:
  TabButton(std::string id, std::string label, bool chosen, Orientation orientation,
            std::function<void()> choose)
      : Widget{std::move(id)}, label_{std::move(label)}, chosen_{chosen}, orientation_{orientation},
        choose_{std::move(choose)} {
  }
  [[nodiscard]] Size measure(LayoutContext &context, float maxWidth) override {
    const Theme &theme = context.theme;
    const float width =
        orientation_ == Orientation::Vertical ? maxWidth : std::min(maxWidth, wholeWidth(context));
    return Size{width, theme.px(theme.spacing.compactRowHeight)};
  }
  void arrange(LayoutContext &context, const Rect &frame) override {
    frame_ = frame;
    const Theme &theme = context.theme;
    // Compared with the measured width itself: subtracting the padding back loses float precision.
    if (frame.width >= wholeWidth(context)) {
      shown_ = label_;
      return;
    }
    shown_ = fitted(context.text, label_, frame.width - theme.px(theme.spacing.pad) * 2.0f,
                    theme.px(theme.type.body));
  }
  [[nodiscard]] bool focusable() const override {
    return true;
  }
  [[nodiscard]] float cornerRadius(const Theme &theme) const override {
    return rowRadius(theme);
  }
  void focused(ActContext & /*context*/) override {
    if (!chosen_ && choose_) {
      choose_();
    }
  }
  bool act(Action action, ActContext &context) override {
    if (action != Action::Accept) {
      return false;
    }
    // A goes on into what the tab shows, the way a direction would.
    context.perform(orientation_ == Orientation::Vertical ? Action::Right : Action::Down);
    return true;
  }
  [[nodiscard]] std::vector<Hint> hints() const override {
    return {Hint{{Action::Accept}, "Open"}};
  }
  void paint(Painter &painter) const override {
    const Theme &theme = painter.theme;
    const bool focused = painter.focused == id();
    Colour fill = theme.colours.control;
    if (focused) {
      fill = theme.colours.controlFocused;
    } else if (chosen_) {
      fill = theme.colours.selected;
    }
    painter.canvas.fillRoundRect(frame_, rowRadius(theme), painter.fade(fill));
    const float centreY = frame_.y + frame_.height * 0.5f;
    if (orientation_ == Orientation::Vertical) {
      painter.label(shown_, Point{frame_.x + painter.px(theme.spacing.pad), centreY},
                    theme.type.body,
                    chosen_ || focused ? theme.colours.ink : theme.colours.inkMuted);
      if (chosen_) {
        const float bar = painter.px(4.0f);
        const float height = frame_.height * 0.4f;
        painter.canvas.fillRoundRect(
            Rect{frame_.x + painter.px(5.0f), centreY - height * 0.5f, bar, height}, bar * 0.5f,
            painter.fade(theme.colours.accent));
      }
      return;
    }
    const float width = painter.text.measure(shown_, painter.px(theme.type.body));
    painter.label(shown_, Point{frame_.x + (frame_.width - width) * 0.5f, centreY}, theme.type.body,
                  chosen_ || focused ? theme.colours.ink : theme.colours.inkMuted);
  }

private:
  [[nodiscard]] float wholeWidth(LayoutContext &context) const {
    const Theme &theme = context.theme;
    return context.text.measure(label_, theme.px(theme.type.body)) +
           theme.px(theme.spacing.pad) * 2.0f;
  }

  std::string label_;
  bool chosen_;
  Orientation orientation_;
  std::function<void()> choose_;
  std::string shown_;
};

} // namespace

Tabs::Tabs(std::string id, std::vector<Tab> tabs, std::size_t chosen,
           std::function<void(std::size_t)> chose, Orientation orientation)
    : Container{std::move(id)}, tabs_{std::move(tabs)},
      chosen_{std::min(chosen, tabs_.empty() ? 0 : tabs_.size() - 1)}, chose_{std::move(chose)},
      orientation_{orientation} {
  for (std::size_t i = 0; i < tabs_.size(); ++i) {
    adopt(std::make_unique<TabButton>(tabId(tabs_[i].key), tabs_[i].label, i == chosen_,
                                      orientation_, [this, i] {
                                        chosen_ = i;
                                        if (chose_) {
                                          chose_(i);
                                        }
                                      }));
  }
}

std::string Tabs::tabId(std::string_view key) const {
  return id() + "/" + std::string{key};
}

std::optional<std::string> Tabs::step(int step) {
  if (tabs_.empty()) {
    return std::nullopt;
  }
  const auto last = static_cast<long>(tabs_.size()) - 1;
  const auto next = static_cast<std::size_t>(
      std::clamp(static_cast<long>(chosen_) + static_cast<long>(step), 0L, last));
  if (next == chosen_) {
    return std::nullopt;
  }
  chosen_ = next;
  if (chose_) {
    chose_(next);
  }
  return tabId(tabs_[next].key);
}

std::optional<Rect> Tabs::clip() const {
  if (!overflows_) {
    return std::nullopt;
  }
  return frame_;
}

std::optional<std::string> Tabs::preferredFocus() const {
  if (tabs_.empty()) {
    return std::nullopt;
  }
  return tabId(tabs_[chosen_].key);
}

Size Tabs::measure(LayoutContext &context, float maxWidth) {
  const Theme &theme = context.theme;
  const float gap = theme.px(spacing_);
  if (orientation_ == Orientation::Vertical) {
    float height = 0.0f;
    for (const std::unique_ptr<Widget> &tab : children_) {
      height += tab->measure(context, maxWidth - padX(theme)).height;
    }
    return Size{maxWidth, height + gaps(theme) + padY(theme)};
  }
  float width = 0.0f;
  float height = 0.0f;
  for (const std::unique_ptr<Widget> &tab : children_) {
    const Size size = tab->measure(context, maxWidth);
    width += size.width;
    height = std::max(height, size.height);
  }
  return Size{std::min(maxWidth, width + gap * static_cast<float>(children_.size()) + padX(theme)),
              height + padY(theme)};
}

void Tabs::arrange(LayoutContext &context, const Rect &frame) {
  frame_ = frame;
  const Theme &theme = context.theme;
  const Rect area = inner(theme, frame);
  float at = orientation_ == Orientation::Vertical ? area.y : area.x;
  overflows_ = false;
  if (orientation_ == Orientation::Horizontal && !children_.empty()) {
    // The strip scrolls so the chosen tab stands whole inside the frame.
    float total = 0.0f;
    float chosenStart = 0.0f;
    float chosenEnd = 0.0f;
    for (std::size_t i = 0; i < children_.size(); ++i) {
      const float width = children_[i]->measure(context, area.width).width;
      if (i == chosen_) {
        chosenStart = total;
        chosenEnd = total + width;
      }
      total += width + (i + 1 < children_.size() ? theme.px(spacing_) : 0.0f);
    }
    overflows_ = total > area.width + 0.5f;
    ViewState::Scroll &scroll = context.state.scroll(id());
    scroll.viewport = area.width;
    scroll.limit = std::max(0.0f, total - area.width);
    float target = std::clamp(scroll.offset.target(), 0.0f, scroll.limit);
    const float margin = theme.px(theme.spacing.pad);
    if (chosenStart - margin < target) {
      target = std::max(0.0f, chosenStart - margin);
    } else if (chosenEnd + margin > target + area.width) {
      target = std::min(scroll.limit, chosenEnd + margin - area.width);
    }
    scroll.offset.aim(overflows_ ? target : 0.0f);
    at -= std::clamp(scroll.offset.step(Timing{context.now, theme.motion.scroll}), 0.0f,
                     scroll.limit);
  }
  for (const std::unique_ptr<Widget> &tab : children_) {
    const Size size = tab->measure(context, area.width);
    if (orientation_ == Orientation::Vertical) {
      tab->arrange(context, Rect{area.x, at, area.width, size.height});
      at += size.height + theme.px(spacing_);
    } else {
      tab->arrange(context, Rect{at, area.y, size.width, area.height});
      at += size.width + theme.px(spacing_);
    }
  }
}

// List

List::List(std::string id, float spacingDp) : ScrollView{std::move(id)} {
  rows_ = &content<Column>();
  rows_->space(spacingDp);
}

// HintBar

Size HintBar::measure(LayoutContext &context, float maxWidth) {
  return Size{maxWidth, context.theme.px(context.theme.spacing.hintBarHeight)};
}

void HintBar::show(std::vector<Hint> hints, const Theme &theme, Typesetter &text,
                   Prompts &prompts) {
  placed_.clear();
  const float size = theme.px(theme.type.hint);
  const float glyph = size * glyphShare;
  const float gap = theme.px(theme.spacing.gap);
  const float between = theme.px(theme.spacing.pad * 1.5f);
  const float pad = theme.px(theme.spacing.pad);
  const float centreY = frame_.y + frame_.height * 0.5f;
  std::vector<Placed> start;
  std::vector<Placed> end;
  for (Hint &hint : hints) {
    Placed placed;
    float width = 0.0f;
    for (const Action key : hint.keys) {
      const float wide = prompts.width(key, glyph);
      placed.glyphs.push_back(Rect{width, centreY - glyph * 0.5f, wide, glyph});
      width += wide + gap * 0.5f;
    }
    placed.labelX = width + gap * 0.5f;
    width = placed.labelX + text.measure(hint.label, size);
    placed.whole = Rect{0.0f, centreY - glyph * 0.5f - gap, width, glyph + 2.0f * gap};
    const bool back = std::ranges::find(hint.keys, Action::Back) != hint.keys.end();
    placed.hint = std::move(hint);
    (back ? start : end).push_back(std::move(placed));
  }
  const auto shift = [](Placed &placed, float x) {
    for (Rect &box : placed.glyphs) {
      box.x += x;
    }
    placed.labelX += x;
    placed.whole.x += x;
  };
  float x = frame_.x + pad;
  for (Placed &placed : start) {
    shift(placed, x);
    x += placed.whole.width + between;
    placed_.push_back(std::move(placed));
  }
  // A bar too narrow for every prompt drops the last ones, the screen's own, first.
  const float room = frame_.right() - pad - x;
  float needed = 0.0f;
  for (const Placed &placed : end) {
    needed += placed.whole.width + between;
  }
  while (!end.empty() && needed - between > room) {
    needed -= end.back().whole.width + between;
    end.pop_back();
  }
  x = frame_.right() - pad;
  for (auto placed = end.rbegin(); placed != end.rend(); ++placed) {
    x -= placed->whole.width;
    shift(*placed, x);
    x -= between;
    placed_.push_back(std::move(*placed));
  }
}

std::vector<Hint> HintBar::shown() const {
  std::vector<Hint> hints;
  hints.reserve(placed_.size());
  for (const Placed &placed : placed_) {
    hints.push_back(placed.hint);
  }
  return hints;
}

bool HintBar::press(Point at, ActContext &context) {
  for (const Placed &placed : placed_) {
    if (contains(placed.whole, at) && !placed.hint.keys.empty()) {
      context.perform(placed.hint.keys.front());
      return true;
    }
  }
  return false;
}

void HintBar::paint(Painter &painter) const {
  const Theme &theme = painter.theme;
  const Colour ink = painter.fade(theme.colours.inkMuted);
  const float centreY = frame_.y + frame_.height * 0.5f;
  for (const Placed &placed : placed_) {
    for (std::size_t i = 0; i < placed.glyphs.size(); ++i) {
      painter.prompts.draw(painter.canvas, placed.hint.keys[i], placed.glyphs[i], ink);
    }
    painter.label(placed.hint.label, Point{placed.labelX, centreY}, theme.type.hint,
                  theme.colours.inkMuted);
  }
}

// Toast

void Toast::show(std::string text, bool error, double now, double seconds) {
  text_ = std::move(text);
  error_ = error;
  shown_ = now;
  until_ = now + seconds;
}

void Toast::clear() noexcept {
  text_.clear();
}

bool Toast::visible(double now) const noexcept {
  return !text_.empty() && now < until_;
}

void Toast::paint(Painter &painter, const Rect &area) const {
  if (!visible(painter.now)) {
    return;
  }
  const Theme &theme = painter.theme;
  const double fade = static_cast<double>(theme.motion.toast) / 1000.0;
  const auto look = static_cast<float>(
      std::clamp(std::min(painter.now - shown_, until_ - painter.now) / fade, 0.0, 1.0));
  const float size = painter.px(theme.type.toast);
  const float padX = painter.px(theme.spacing.pad * 1.3f);
  const float padY = painter.px(theme.spacing.pad * 0.7f);
  const float room = std::max(0.0f, area.width - 4.0f * padX);
  const std::string shown = fitted(painter.text, text_, room, size);
  const float width = painter.text.measure(shown, size) + 2.0f * padX;
  const float height = painter.text.lineHeight(size) + 2.0f * padY;
  const float rise = painter.px(12.0f) * (1.0f - easeOut(look));
  const Rect box{area.x + (area.width - width) * 0.5f, area.bottom() - height + rise, width,
                 height};
  const float alpha = painter.alpha;
  painter.alpha *= look;
  painter.canvas.fillRoundRect(
      box, height * 0.5f, painter.fade(error_ ? theme.colours.toastError : theme.colours.toast));
  painter.label(shown, Point{box.x + padX, box.y + height * 0.5f}, theme.type.toast,
                theme.colours.onToast);
  painter.alpha = alpha;
}

} // namespace gpu2d::ui
