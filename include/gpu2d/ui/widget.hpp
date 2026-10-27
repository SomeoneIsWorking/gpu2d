// widget — the node of a screen's tree. A screen builds its tree again whenever it may have
// changed; each node is measured and arranged once per layout, and painting, hit testing and focus
// all read the frame that layout gave it. What must outlive a rebuild (focus, scroll offsets, the
// child a group last focused) lives in the screen's `ViewState`, keyed by widget id.
#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gpu2d/canvas.hpp"
#include "gpu2d/geometry.hpp"
#include "gpu2d/ui/input.hpp"
#include "gpu2d/ui/motion.hpp"
#include "gpu2d/ui/theme.hpp"
#include "gpu2d/ui/typesetter.hpp"

namespace gpu2d::ui {

class Screen;

struct Size {
  float width{};
  float height{};
};

/// Room kept inside a container's edges, in dp.
struct Insets {
  float left{};
  float top{};
  float right{};
  float bottom{};

  [[nodiscard]] static Insets all(float dp) noexcept {
    return Insets{dp, dp, dp, dp};
  }
  [[nodiscard]] static Insets symmetric(float horizontal, float vertical) noexcept {
    return Insets{horizontal, vertical, horizontal, vertical};
  }
};

/// What a screen keeps across rebuilds of its tree.
class ViewState {
public:
  struct Scroll {
    Follow offset;
    /// The furthest the content scrolls, in pixels.
    float limit{};
    float viewport{};
  };

  [[nodiscard]] Scroll &scroll(const std::string &id) {
    return scrolls_[id];
  }
  [[nodiscard]] const Scroll *findScroll(const std::string &id) const;

  /// The focused widget's id; empty before the screen first lays out.
  std::string focused;
  /// Where the focused widget stood when last laid out, to land near it if it goes away.
  Rect focusFrame;
  /// Whether the pointer moved the focus last, which scrolls nothing into view.
  bool focusFromPointer{false};
  /// The id of the widget the pointer rests on; empty for none.
  std::string hovered;
  /// Per focus group, the id of the child it last focused.
  std::map<std::string, std::string, std::less<>> remembered;
  /// The focus ring as drawn.
  RectFollow ring;
  /// The id of the text field being typed in, which holds the focus; empty for none.
  std::string editing;
  /// That field's caret, a byte offset clamped to its text when laid out.
  std::size_t caret{};

private:
  std::map<std::string, Scroll, std::less<>> scrolls_;
};

/// What laying out reads.
struct LayoutContext {
  const Theme &theme;
  Typesetter &text;
  ViewState &state;
  double now{};
};

enum class Sound : std::uint8_t { Move, Activate, Back };

/// What a widget may ask while it handles input. Screens pushed or popped here change the stack
/// once the input is handled, never under the widget handling it.
class ActContext {
public:
  ActContext() = default;
  virtual ~ActContext() = default;
  ActContext(const ActContext &) = delete;
  ActContext &operator=(const ActContext &) = delete;
  ActContext(ActContext &&) = delete;
  ActContext &operator=(ActContext &&) = delete;

  virtual void push(std::unique_ptr<Screen> screen) = 0;
  /// Pops the screen the input went to.
  virtual void pop() = 0;
  virtual void sound(Sound sound) = 0;
  /// Handles `action` as if pressed (a hint clicked).
  virtual void perform(Action action) = 0;
  /// Moves the focus to the widget `id` once the input is handled.
  virtual void focus(std::string id) = 0;
  /// Asks the app's on-screen keyboard to type into the focused field (`Navigator::setKeyboard`).
  virtual void openKeyboard() = 0;
  [[nodiscard]] virtual double now() const = 0;
  [[nodiscard]] virtual const Theme &theme() const = 0;
};

/// What painting reads and draws on.
struct Painter {
  Canvas &canvas;
  const Theme &theme;
  Typesetter &text;
  Prompts &prompts;
  /// The focused widget's id on the screen being painted; empty when it has none.
  std::string_view focused;
  /// The id of the widget the pointer rests on; empty when none.
  std::string_view hovered;
  double now{};
  /// The screen's opacity as it comes and goes.
  float alpha{1.0f};
  /// What can be seen through every clip above; children wholly outside it are skipped.
  Rect visible;

  [[nodiscard]] Colour fade(Colour colour) const noexcept {
    return faded(colour, alpha);
  }
  [[nodiscard]] float px(float dp) const noexcept {
    return theme.px(dp);
  }
  /// Draws `words` at `sizeDp` from `start.x`, the line centred on `start.y`.
  void label(std::string_view words, Point start, float sizeDp, Colour colour) const;
  /// Draws `words` at `sizeDp` ending at `end.x`, the line centred on `end.y`; returns its left
  /// edge.
  float labelEndingAt(std::string_view words, Point end, float sizeDp, Colour colour) const;
};

class Widget {
public:
  explicit Widget(std::string id = {}) : id_{std::move(id)} {
  }
  virtual ~Widget() = default;
  Widget(const Widget &) = delete;
  Widget &operator=(const Widget &) = delete;
  Widget(Widget &&) = delete;
  Widget &operator=(Widget &&) = delete;

  /// Stable across rebuilds; empty for a widget nothing refers to.
  [[nodiscard]] const std::string &id() const noexcept {
    return id_;
  }

  /// Fixes the width, in dp.
  Widget &fixWidth(float dp) noexcept {
    fixedWidth_ = dp;
    return *this;
  }
  /// Fixes the height, in dp.
  Widget &fixHeight(float dp) noexcept {
    fixedHeight_ = dp;
    return *this;
  }
  /// Takes `weight` shares of the room its container has to spare along its axis.
  Widget &grow(float weight = 1.0f) noexcept {
    grow_ = weight;
    return *this;
  }
  [[nodiscard]] std::optional<float> fixedWidth() const noexcept {
    return fixedWidth_;
  }
  [[nodiscard]] std::optional<float> fixedHeight() const noexcept {
    return fixedHeight_;
  }
  [[nodiscard]] float growth() const noexcept {
    return grow_;
  }

  /// The size it wants no wider than `maxWidth` pixels.
  [[nodiscard]] virtual Size measure(LayoutContext &context, float maxWidth);
  /// Takes `frame` and places its children.
  virtual void arrange(LayoutContext &context, const Rect &frame);
  [[nodiscard]] const Rect &frame() const noexcept {
    return frame_;
  }

  [[nodiscard]] std::span<const std::unique_ptr<Widget>> children() const noexcept {
    return children_;
  }
  /// The area its children are clipped to, or nothing.
  [[nodiscard]] virtual std::optional<Rect> clip() const {
    return std::nullopt;
  }
  /// Whether focus coming from outside lands on the child it last focused.
  [[nodiscard]] virtual bool remembersFocus() const {
    return false;
  }
  /// The child focus coming from outside lands on, before any remembered one.
  [[nodiscard]] virtual std::optional<std::string> preferredFocus() const {
    return std::nullopt;
  }

  [[nodiscard]] virtual bool focusable() const {
    return false;
  }
  /// Whether a pointer press reaches it though it takes no focus.
  [[nodiscard]] virtual bool pressable() const {
    return false;
  }
  /// The corner radius the focus ring follows, in pixels.
  [[nodiscard]] virtual float cornerRadius(const Theme &theme) const;
  /// Handles `action` while focused; false leaves it to the screen.
  virtual bool act(Action action, ActContext &context);
  /// A pointer press at `at`, inside its frame, after it took the focus. Presses it by default.
  virtual bool press(Point at, ActContext &context);
  /// Whether typed text goes to it while it is focused.
  [[nodiscard]] virtual bool takesText() const {
    return false;
  }
  /// Typed text while focused; true when taken.
  virtual bool type(std::string_view /*typed*/, ActContext & /*context*/) {
    return false;
  }
  /// What its actions are called, for the hint bar.
  [[nodiscard]] virtual std::vector<Hint> hints() const {
    return {};
  }
  /// Called when a focus or a pointer lands on it from elsewhere.
  virtual void focused(ActContext & /*context*/) {
  }
  /// The pointer moved to `at`, inside its frame, after it took any focus.
  virtual void hover(Point /*at*/, ActContext & /*context*/) {
  }
  /// Wheel notches while focused with no scroll view under the pointer; true when taken.
  virtual bool wheel(float /*notches*/, ActContext & /*context*/) {
    return false;
  }
  /// A secondary press at `at`, inside its frame, after it took the focus; false leaves it to the
  /// focused widget's and the screen's second action.
  virtual bool context(Point /*at*/, ActContext & /*context*/) {
    return false;
  }
  /// Whether it draws its own focus, so the navigator's ring stays off it.
  [[nodiscard]] virtual bool drawsFocus() const {
    return false;
  }

  /// Draws it, then its children.
  virtual void paint(Painter &painter) const;

protected:
  void paintChildren(Painter &painter) const;
  template <typename W> W &adopt(std::unique_ptr<W> child) {
    W &added = *child;
    children_.push_back(std::move(child));
    return added;
  }

  std::vector<std::unique_ptr<Widget>> children_;
  Rect frame_;

private:
  std::string id_;
  std::optional<float> fixedWidth_;
  std::optional<float> fixedHeight_;
  float grow_{0.0f};
};

/// Whether `point` lies in `rect`.
[[nodiscard]] bool contains(const Rect &rect, Point point) noexcept;
/// The part of `a` inside `b`; empty when they do not meet.
[[nodiscard]] Rect intersection(const Rect &a, const Rect &b) noexcept;

} // namespace gpu2d::ui
