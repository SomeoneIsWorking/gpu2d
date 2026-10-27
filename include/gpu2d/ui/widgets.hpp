// widgets — the standard widgets, each with one behaviour wherever it is used:
// - Label and SectionHeading: words; nothing to press.
// - Button: A presses it.
// - CardButton: a card holding a picture and its caption; A presses it, a marked one is ticked.
// - Item (a settings row: title, subtitle, value, an optional chip naming where the value comes
//   from): A presses it when it does something; one with nothing to do is not focusable. X and Y
//   do the row's second and third actions when it has them, on every kind of row.
// - TextField: typed text goes in at the end, Erase takes the last character off, Y clears it and
//   Submit is done; A asks the app's on-screen keyboard to type into it.
// - Toggle: A flips it.
// - Choice: Left and Right step through its options; A opens them as a list.
// - Slider: Left and Right step it; a press on the track sets it.
// - Tabs: LB and RB step them from anywhere on the screen; focusing a tab chooses it.
// - HintBar: the prompts for the focused widget and the screen, never written by hand.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gpu2d/ui/layout.hpp"
#include "gpu2d/ui/widget.hpp"

namespace gpu2d::ui {

enum class TextRole : std::uint8_t { Title, Heading, Body, Caption };

/// Words in one of the theme's sizes, fitted to one line or wrapped.
class Label : public Widget {
public:
  Label(std::string text, TextRole role, bool wraps = false)
      : text_{std::move(text)}, role_{role}, wraps_{wraps} {
  }
  Label &colour(Colour colour) noexcept {
    colour_ = colour;
    return *this;
  }
  [[nodiscard]] Size measure(LayoutContext &context, float maxWidth) override;
  void arrange(LayoutContext &context, const Rect &frame) override;
  void paint(Painter &painter) const override;

private:
  [[nodiscard]] float sizeDp(const Theme &theme) const noexcept;
  [[nodiscard]] Colour ink(const Theme &theme) const noexcept;

  std::string text_;
  TextRole role_;
  bool wraps_;
  std::optional<Colour> colour_;
  std::vector<std::string> lines_;
};

/// A heading over a group of rows, with the theme's section room above it.
class SectionHeading : public Widget {
public:
  explicit SectionHeading(std::string text) : text_{std::move(text)} {
  }
  [[nodiscard]] Size measure(LayoutContext &context, float maxWidth) override;
  void paint(Painter &painter) const override;

private:
  std::string text_;
};

enum class ButtonKind : std::uint8_t { Normal, Primary, Danger };

class Button : public Widget {
public:
  Button(std::string id, std::string label, std::function<void()> pressed,
         ButtonKind kind = ButtonKind::Normal)
      : Widget{std::move(id)}, label_{std::move(label)}, pressed_{std::move(pressed)}, kind_{kind} {
  }
  [[nodiscard]] Size measure(LayoutContext &context, float maxWidth) override;
  [[nodiscard]] bool focusable() const override {
    return true;
  }
  [[nodiscard]] float cornerRadius(const Theme &theme) const override;
  bool act(Action action, ActContext &context) override;
  [[nodiscard]] std::vector<Hint> hints() const override;
  void paint(Painter &painter) const override;

private:
  std::string label_;
  std::function<void()> pressed_;
  ButtonKind kind_;
};

/// A labelled line of text being typed: what is typed, or a placeholder while it is empty, with a
/// caret while focused; a masked field shows a dot per character. A, typed text or Erase start
/// editing it with the caret at the end; while edited, Left and Right move the caret a character,
/// typing and Erase act at it, and Submit or the focus leaving stop it. The caret lives in the
/// screen's `ViewState`. It edits the app's text through `changed`, so it holds a copy only while
/// built; a masked copy is wiped.
class TextField : public Widget {
public:
  TextField(std::string id, std::string label, std::string text,
            std::function<void(std::string)> changed)
      : Widget{std::move(id)}, label_{std::move(label)}, text_{std::move(text)},
        changed_{std::move(changed)} {
  }
  ~TextField() override;
  TextField(const TextField &) = delete;
  TextField &operator=(const TextField &) = delete;
  TextField(TextField &&) = delete;
  TextField &operator=(TextField &&) = delete;

  /// What the empty field says.
  TextField &placeholder(std::string words) {
    placeholder_ = std::move(words);
    return *this;
  }
  /// Shows a dot per character, for a password.
  TextField &mask(bool masked = true) noexcept {
    masked_ = masked;
    return *this;
  }
  /// The most bytes it holds; typing that would pass it is refused whole.
  TextField &limit(std::size_t bytes) noexcept {
    limit_ = bytes;
    return *this;
  }
  /// What Submit does, and the hint's word for it.
  TextField &onSubmit(std::function<void()> submitted, std::string verb = "Done") {
    submitted_ = std::move(submitted);
    verb_ = std::move(verb);
    return *this;
  }

  [[nodiscard]] const std::string &text() const noexcept {
    return text_;
  }
  [[nodiscard]] bool masked() const noexcept {
    return masked_;
  }

  [[nodiscard]] Size measure(LayoutContext &context, float maxWidth) override;
  void arrange(LayoutContext &context, const Rect &frame) override;
  [[nodiscard]] bool focusable() const override {
    return true;
  }
  [[nodiscard]] bool takesText() const override {
    return true;
  }
  [[nodiscard]] float cornerRadius(const Theme &theme) const override;
  bool act(Action action, ActContext &context) override;
  bool type(std::string_view typed, ActContext &context) override;
  [[nodiscard]] std::vector<Hint> hints() const override;
  void paint(Painter &painter) const override;

private:
  /// The line as drawn: the text, or a dot per character when masked.
  [[nodiscard]] std::string drawn() const;
  /// Starts editing with the caret at the end, unless it is being edited.
  void edit();
  /// Puts the caret at byte `at` and the field's text to `next`.
  void change(std::string next, std::size_t at);

  std::string label_;
  std::string text_;
  std::function<void(std::string)> changed_;
  std::string placeholder_;
  bool masked_{false};
  std::size_t limit_{SIZE_MAX};
  std::function<void()> submitted_;
  std::string verb_;
  std::string labelShown_;
  std::string lineShown_;
  /// The screen's state as last laid out, which holds the caret.
  ViewState *state_{nullptr};
  bool editing_{false};
  /// The caret as a byte offset into the text, and into `lineShown_`.
  std::size_t caret_{};
  std::size_t caretShown_{};
};

/// A row: title, an optional subtitle, and what stands at its end. A plain item shows a value and,
/// when it opens something, a chevron; Toggle, Choice and Slider put their control there.
class Item : public Widget {
public:
  struct Text {
    std::string title;
    std::string subtitle;
  };

  Item(std::string id, Text text) : Widget{std::move(id)}, text_{std::move(text)} {
  }
  /// The value read at the row's end.
  Item &value(std::string value) {
    value_ = std::move(value);
    return *this;
  }
  /// What A does, and the hint's word for it; `opens` draws a chevron for a row that leads on.
  Item &onPress(std::function<void()> pressed, std::string verb = "Select", bool opens = false) {
    pressed_ = std::move(pressed);
    verb_ = std::move(verb);
    opens_ = opens;
    return *this;
  }
  /// Makes A ask first: a `confirm` dialog titled `question`, saying `detail`, whose `verb` button
  /// does the press. For a step that throws something away or cannot be undone.
  Item &confirmFirst(std::string question, std::string detail, std::string verb) {
    confirm_ = Confirm{std::move(question), std::move(detail), std::move(verb)};
    return *this;
  }
  /// A short pill before the end naming where the value comes from; `strong` when the row's own.
  Item &chip(std::string text, bool strong) {
    chip_ = std::move(text);
    chipStrong_ = strong;
    return *this;
  }
  /// What `key` (Secondary or Tertiary) does, and the hint's word for it.
  Item &onAction(Action key, std::function<void()> action, std::string verb) {
    extras_.push_back(Extra{key, std::move(action), std::move(verb)});
    return *this;
  }
  /// Puts a tick at its end: the option chosen in a list.
  Item &mark(bool marked = true) noexcept {
    marked_ = marked;
    return *this;
  }
  /// Dims it and takes it out of the focus order.
  Item &disable(bool disabled = true) noexcept {
    disabled_ = disabled;
    return *this;
  }

  [[nodiscard]] Size measure(LayoutContext &context, float maxWidth) override;
  void arrange(LayoutContext &context, const Rect &frame) override;
  [[nodiscard]] bool focusable() const override;
  [[nodiscard]] float cornerRadius(const Theme &theme) const override;
  bool act(Action action, ActContext &context) override;
  [[nodiscard]] std::vector<Hint> hints() const override;
  void paint(Painter &painter) const override;

  [[nodiscard]] const Text &text() const noexcept {
    return text_;
  }
  [[nodiscard]] const std::string &valueText() const noexcept {
    return value_;
  }
  [[nodiscard]] const std::string &chipText() const noexcept {
    return chip_;
  }

protected:
  /// The row's own action for `action` (X, Y), when it has one; false otherwise.
  bool actExtra(Action action);
  /// The prompts of those actions.
  void addExtraHints(std::vector<Hint> &hints) const;
  /// How wide the end's control is, in pixels, for a row `width` wide.
  [[nodiscard]] virtual float trailingWidth(LayoutContext &context, float width);
  /// Draws the end's control in `area`.
  virtual void paintTrailing(Painter &painter, const Rect &area) const;
  [[nodiscard]] bool interactive() const noexcept {
    return !disabled_;
  }
  [[nodiscard]] bool disabled() const noexcept {
    return disabled_;
  }
  /// The end's area as laid out.
  [[nodiscard]] const Rect &trailing() const noexcept {
    return trailing_;
  }

private:
  Text text_;
  std::string value_;
  std::function<void()> pressed_;
  std::string verb_;
  bool opens_{false};
  struct Confirm {
    std::string question;
    std::string detail;
    std::string verb;
  };
  std::optional<Confirm> confirm_;
  bool marked_{false};
  bool disabled_{false};
  std::string chip_;
  bool chipStrong_{false};
  struct Extra {
    Action key;
    std::function<void()> action;
    std::string verb;
  };
  std::vector<Extra> extras_;
  Rect trailing_;
  Rect chipBox_;
  std::string chipShown_;
  /// The title and subtitle fitted to the room left of the end.
  std::string titleShown_;
  std::string subtitleShown_;
  std::string valueShown_;
};

class Toggle : public Item {
public:
  Toggle(std::string id, Text text, bool on, std::function<void(bool)> changed)
      : Item{std::move(id), std::move(text)}, on_{on}, changed_{std::move(changed)} {
  }
  [[nodiscard]] bool focusable() const override {
    return interactive();
  }
  bool act(Action action, ActContext &context) override;
  [[nodiscard]] std::vector<Hint> hints() const override;
  [[nodiscard]] bool on() const noexcept {
    return on_;
  }

protected:
  [[nodiscard]] float trailingWidth(LayoutContext &context, float width) override;
  void paintTrailing(Painter &painter, const Rect &area) const override;

private:
  bool on_;
  std::function<void(bool)> changed_;
};

class Choice : public Item {
public:
  /// `chosen` indexes `options`; `changed` gets the index picked.
  Choice(std::string id, Text text, std::vector<std::string> options, std::size_t chosen,
         std::function<void(std::size_t)> changed)
      : Item{std::move(id), std::move(text)}, options_{std::move(options)}, chosen_{chosen},
        changed_{std::move(changed)} {
  }
  /// Words for the options in the list A opens, where they say more than the row's value
  /// ("Use 4:3 from Global" for a value read as "4:3"); one per option.
  Choice &listAs(std::vector<std::string> labels) {
    listed_ = std::move(labels);
    return *this;
  }
  [[nodiscard]] bool focusable() const override {
    return interactive() && !options_.empty();
  }
  bool act(Action action, ActContext &context) override;
  bool press(Point at, ActContext &context) override;
  [[nodiscard]] std::vector<Hint> hints() const override;
  [[nodiscard]] const std::vector<std::string> &options() const noexcept {
    return options_;
  }
  [[nodiscard]] std::size_t chosen() const noexcept {
    return chosen_;
  }

protected:
  [[nodiscard]] float trailingWidth(LayoutContext &context, float width) override;
  void paintTrailing(Painter &painter, const Rect &area) const override;

private:
  /// Moves `step` places, wrapping.
  void stepBy(int step, ActContext &context);
  /// Opens the options as a list over the screen.
  void openList(ActContext &context);

  std::vector<std::string> options_;
  std::vector<std::string> listed_;
  std::size_t chosen_;
  std::function<void(std::size_t)> changed_;
  std::string shown_;
};

class Slider : public Item {
public:
  struct Range {
    int low{};
    int high{};
    int step{1};
  };

  Slider(std::string id, Text text, Range range, int level, std::function<void(int)> changed)
      : Item{std::move(id), std::move(text)}, range_{range}, level_{level},
        changed_{std::move(changed)} {
  }
  /// What follows the number ("%").
  Slider &unit(std::string unit) {
    unit_ = std::move(unit);
    return *this;
  }
  [[nodiscard]] bool focusable() const override {
    return interactive();
  }
  bool act(Action action, ActContext &context) override;
  bool press(Point at, ActContext &context) override;
  [[nodiscard]] std::vector<Hint> hints() const override;
  [[nodiscard]] int level() const noexcept {
    return level_;
  }

protected:
  [[nodiscard]] float trailingWidth(LayoutContext &context, float width) override;
  void paintTrailing(Painter &painter, const Rect &area) const override;

private:
  /// The track inside the end's area, laid out.
  [[nodiscard]] Rect track(const Theme &theme) const noexcept;
  void set(int level, ActContext &context);

  Range range_;
  int level_;
  std::function<void(int)> changed_;
  std::string unit_;
};

enum class Orientation : std::uint8_t { Horizontal, Vertical };

/// Tabs choosing what the rest of the screen shows. Each tab is focusable; focusing one chooses
/// it, and LB and RB step them wherever the focus is. A horizontal strip wider than its frame
/// scrolls the chosen tab into view rather than cutting a label short.
class Tabs : public Container {
public:
  struct Tab {
    std::string key;
    std::string label;
  };

  Tabs(std::string id, std::vector<Tab> tabs, std::size_t chosen,
       std::function<void(std::size_t)> chose, Orientation orientation);

  /// The id of tab `key`.
  [[nodiscard]] std::string tabId(std::string_view key) const;
  [[nodiscard]] std::size_t chosen() const noexcept {
    return chosen_;
  }
  [[nodiscard]] const std::vector<Tab> &tabs() const noexcept {
    return tabs_;
  }
  /// Chooses the tab `step` places on, stopping at the ends; the id of the tab chosen, or nothing
  /// when it did not move.
  std::optional<std::string> step(int step);

  [[nodiscard]] Size measure(LayoutContext &context, float maxWidth) override;
  void arrange(LayoutContext &context, const Rect &frame) override;
  [[nodiscard]] bool remembersFocus() const override {
    return true;
  }
  [[nodiscard]] std::optional<std::string> preferredFocus() const override;
  [[nodiscard]] std::optional<Rect> clip() const override;

private:
  std::vector<Tab> tabs_;
  /// A horizontal strip is wider than its frame.
  bool overflows_{false};
  std::size_t chosen_;
  std::function<void(std::size_t)> chose_;
  Orientation orientation_;
};

/// A card A presses: what it holds (a picture and its caption) is one choice, as in a grid of
/// pictures. Drawn as a row is; a marked one has a tick in its corner, the one chosen now.
class CardButton : public Card {
public:
  /// `verb` is the hint's word for A.
  CardButton(std::string id, std::function<void()> pressed, std::string verb = "Select")
      : Card{std::move(id)}, pressed_{std::move(pressed)}, verb_{std::move(verb)} {
  }
  CardButton &mark(bool marked = true) noexcept {
    marked_ = marked;
    return *this;
  }
  [[nodiscard]] bool marked() const noexcept {
    return marked_;
  }

  [[nodiscard]] bool focusable() const override {
    return true;
  }
  [[nodiscard]] float cornerRadius(const Theme &theme) const override;
  bool act(Action action, ActContext &context) override;
  [[nodiscard]] std::vector<Hint> hints() const override;
  void paint(Painter &painter) const override;

private:
  std::function<void()> pressed_;
  std::string verb_;
  bool marked_{false};
};

/// How far something has got: a thin bar filled from the left, or with no measure, a stretch of
/// it sweeping across while the work goes on.
class Progress : public Widget {
public:
  /// `fraction` is 0 to 1; nothing when the work has no measure.
  explicit Progress(std::optional<float> fraction) noexcept : fraction_{fraction} {
  }
  [[nodiscard]] Size measure(LayoutContext &context, float maxWidth) override;
  void paint(Painter &painter) const override;

private:
  std::optional<float> fraction_;
};

/// A scrolled column of rows.
class List : public ScrollView {
public:
  List(std::string id, float spacingDp);
  /// The column the rows go in.
  [[nodiscard]] Column &rows() noexcept {
    return *rows_;
  }

private:
  Column *rows_;
};

/// The prompts for what is focused and for the screen, placed by the navigator after layout: the
/// Back prompt at the start, the rest at the end. A press on a prompt does its action.
class HintBar : public Widget {
public:
  HintBar() = default;
  [[nodiscard]] Size measure(LayoutContext &context, float maxWidth) override;
  [[nodiscard]] bool pressable() const override {
    return true;
  }
  bool press(Point at, ActContext &context) override;
  void paint(Painter &painter) const override;

  /// Lays `hints` out across the bar.
  void show(std::vector<Hint> hints, const Theme &theme, Typesetter &text, Prompts &prompts);
  /// The prompts it shows, in order.
  [[nodiscard]] std::vector<Hint> shown() const;

private:
  struct Placed {
    Hint hint;
    /// Each key's glyph box, then the label's start.
    std::vector<Rect> glyphs;
    float labelX{};
    Rect whole;
  };
  std::vector<Placed> placed_;
};

/// A short message low on the screen that fades in and out.
class Toast {
public:
  /// Shows `text` from `now` for `seconds`.
  void show(std::string text, bool error, double now, double seconds);
  void clear() noexcept;
  [[nodiscard]] bool visible(double now) const noexcept;
  [[nodiscard]] const std::string &text() const noexcept {
    return text_;
  }
  [[nodiscard]] bool error() const noexcept {
    return error_;
  }
  /// Draws it centred across `area`, its bottom `area.bottom()`.
  void paint(Painter &painter, const Rect &area) const;

private:
  std::string text_;
  bool error_{false};
  double shown_{};
  double until_{};
};

} // namespace gpu2d::ui
