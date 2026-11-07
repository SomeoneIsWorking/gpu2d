// navigator — the one stack of screens. Input, the pointer, drawing, prompts and typed text all go
// to it and walk it from the top: only the top screen takes input; drawing starts at the highest
// screen that covers the viewport and goes up. Back pops unless a widget or the screen takes it.
// An app's home is the root: the bottom screen, which nothing pops. Typed text goes to the
// focused widget, then the screen; A on a text field asks the app's on-screen keyboard, which
// types back through `text` and `act`. The app's chrome (a top bar, a docked keyboard) are
// screens outside the stack that the pointer reaches first and the app draws where it likes.
// Pushes and pops fade and slide on the toolkit's clock, which the app sets each frame; the
// focus ring eases between widgets and scroll views ease the focus into view.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gpu2d/canvas.hpp"
#include "gpu2d/ui/focus.hpp"
#include "gpu2d/ui/input.hpp"
#include "gpu2d/ui/motion.hpp"
#include "gpu2d/ui/screen.hpp"
#include "gpu2d/ui/tab_dock.hpp"
#include "gpu2d/ui/theme.hpp"
#include "gpu2d/ui/typesetter.hpp"
#include "gpu2d/ui/widget.hpp"

namespace gpu2d::ui {

class Navigator {
public:
  /// What the app plays at each kind of moment; any may be empty.
  struct Sounds {
    std::function<void()> move;
    std::function<void()> activate;
    std::function<void()> back;
  };

  Navigator(Typesetter &text, Prompts &prompts, Sounds sounds)
      : text_{text}, prompts_{prompts}, sounds_{std::move(sounds)} {
  }

  /// The theme in force, set again when the app's scale or palette changes.
  void setTheme(const Theme &theme) noexcept {
    theme_ = theme;
  }
  [[nodiscard]] const Theme &theme() const noexcept {
    return theme_;
  }
  /// What plays at each moment, when the app's sounds come after the navigator.
  void setSounds(Sounds sounds) {
    sounds_ = std::move(sounds);
  }
  /// What A on a text field asks: the app opens its on-screen keyboard over the field.
  void setKeyboard(std::function<void()> open) {
    keyboard_ = std::move(open);
  }
  /// Where pasted text comes from: the app reads its system's clipboard.
  void setClipboard(std::function<std::string()> read) {
    clipboard_ = std::move(read);
  }
  /// The clock, in seconds; set before laying out each frame.
  void setTime(double now) noexcept {
    now_ = now;
  }
  [[nodiscard]] double now() const noexcept {
    return now_;
  }
  /// Whether the screens show touch controls (`LayoutContext::touch`), which the app sets from
  /// the device last used.
  void setTouch(bool touch) noexcept {
    touch_ = touch;
  }
  [[nodiscard]] bool touch() const noexcept {
    return touch_;
  }

  /// Puts `root` under every screen for good, in place of any root before it: `pop` and `clear`
  /// leave it, and Back it does not take does nothing.
  void setRoot(std::unique_ptr<Screen> root);
  void push(std::unique_ptr<Screen> screen);
  /// Pops the top screen, unless it is the root; it fades out.
  void pop();
  /// Puts `screen` in the top one's place, or above the root.
  void replace(std::unique_ptr<Screen> screen);
  /// Pops every screen above the root.
  void clear();
  /// Closes `screen` and every screen above it, the root excepted; asked while input is handled,
  /// once it is done.
  void closeFrom(const Screen &screen);

  /// Whether a screen above the root is open.
  [[nodiscard]] bool active() const noexcept {
    return stack_.size() > base();
  }
  /// Whether the root is the top screen.
  [[nodiscard]] bool atRoot() const noexcept {
    return root_ && stack_.size() == 1;
  }
  /// Whether anything above the root is drawn, which includes screens fading out.
  [[nodiscard]] bool visible() const noexcept;
  /// Whether a screen above the root that is not an overlay is drawn, fading out included.
  [[nodiscard]] bool pagesVisible() const noexcept;
  /// Whether something is still moving.
  [[nodiscard]] bool animating() const noexcept;
  [[nodiscard]] Screen *top() noexcept;
  [[nodiscard]] const Screen *top() const noexcept;
  /// The open screens, bottom first, the root among them.
  [[nodiscard]] std::vector<const Screen *> screens() const;

  /// Adds `chrome`, a screen of the app's own outside the stack: laid out over the viewport after
  /// the stack, drawn by `paintChrome` where the app draws it, and reached by the pointer before
  /// the top screen wherever one of its widgets is (the last added first); what a press there
  /// does to the stack happens at once. It takes no focus of the pad's and must outlive its use
  /// here.
  void addChrome(Screen &chrome);
  /// Draws `chrome` as last laid out.
  void paintChrome(Canvas &canvas, const Screen &chrome);

  /// Builds and lays out the screens drawn in `viewport`; overlays stand above the bottom
  /// `covered` pixels, which a docked keyboard takes. Screens and chrome lay out inside `safe`
  /// (pixels in from each edge: a display cutout, rounded corners), and backdrops still cover the
  /// whole viewport. A press held still past the long-press time becomes a `Context` here, once a
  /// frame.
  void layout(const Rect &viewport, float covered = 0.0f, const Insets &safe = {});
  /// Draws them; after `layout`.
  void paint(Canvas &canvas);
  /// Draws the screens up to the highest one that is not an overlay, for an app that draws its
  /// own chrome over pages and under overlays; `paintOverlays` then draws the rest.
  void paintPages(Canvas &canvas);
  void paintOverlays(Canvas &canvas);

  /// An action for the top screen; true when something took it.
  bool act(Action action);
  bool pointer(const PointerEvent &event);
  /// Typed text for the focused widget, else the top screen; true when something took it.
  bool text(std::string_view typed);
  /// The clipboard's text for the focused widget, else the top screen; true when something took
  /// it.
  bool paste();
  /// Whether the top screen's focused widget takes typed text, as last laid out.
  [[nodiscard]] bool typing() const;
  /// Whether that widget is being edited: typed into, or opened with A, since it took the focus.
  [[nodiscard]] bool editing() const;
  /// Starts editing the top screen's focused text field with its caret at the end, as A on it
  /// does without asking for the keyboard; `false` stops. Asked while input is
  /// handled or before the top screen is laid out, it waits until it is.
  void edit(bool on);
  /// The top screen's prompts: the focused widget's, then the screen's.
  [[nodiscard]] std::vector<Hint> hints();

  /// The focused widget on the top screen; empty when none.
  [[nodiscard]] std::string focused() const;
  /// Focuses the widget `id` on the top screen, scrolling it into view; false when it has none.
  /// Asked while input is handled, it waits until the input is done.
  bool focus(std::string_view id);
  /// The widget `id` on the top screen as last laid out, or null.
  [[nodiscard]] const Widget *find(std::string_view id);

private:
  struct Entry {
    std::unique_ptr<Screen> screen;
    std::unique_ptr<Widget> root;
    Rect area;
    Tween presence;
    bool leaving{false};
  };
  struct Chrome {
    Screen *screen{};
    std::unique_ptr<Widget> root;
  };
  class Context;
  /// A press from `Down` to `Up`: still, it is a tap or a long press; moved past the slop, a drag
  /// of the widget or scroll view that took it.
  struct Gesture {
    enum class Drags : std::uint8_t { Nothing, Widget, Scroll };
    Point start;
    Point last;
    double since{};
    /// It went down on the chrome, which takes taps alone.
    bool chrome{false};
    /// It moved past the slop.
    bool moved{false};
    /// It was held long enough to be a long press.
    bool held{false};
    Drags drags{Drags::Nothing};
    /// The id of what it drags.
    std::string target;
    /// Recent points and when they were reached, for the speed it is let go at.
    std::vector<std::pair<double, Point>> trail;
  };
  /// A change to the stack asked for while input was being handled, applied after it.
  struct Deferred {
    enum class Kind : std::uint8_t { Push, Pop, Replace, Clear };
    Kind kind{};
    std::unique_ptr<Screen> screen;
  };

  /// The viewport inside `safe_`.
  [[nodiscard]] Rect safeArea() const noexcept;
  /// Builds and arranges `entry`'s tree, then settles its focus and scroll.
  void build(Entry &entry);
  /// Rebuilds the top screen with the last viewport; null when there is none.
  Entry *fresh();
  /// Moves the focus `direction` from the focused widget.
  bool move(Entry &entry, Direction direction);
  /// Scrolls the view holding the focus (with none, the first that scrolls) `rows` rows, for
  /// what lies past the last widget that takes focus.
  bool nudge(Entry &entry, int rows);
  /// Focuses `id` on `entry`; `pointer` when the pointer moved it.
  void focusOn(Entry &entry, const std::string &id, bool pointer);
  /// Scrolls every view holding the focused widget so it shows.
  void reveal(Entry &entry);
  /// Whether a view holding the focused widget got shorter in the last layout.
  [[nodiscard]] bool focusViewShrank(Entry &entry) const;
  /// Applies what the handled input asked for.
  void settle(Context &context);
  /// Starts or stops editing `entry`'s focused text field.
  void applyEdit(Entry &entry, bool on);
  /// Moves the screens that asked to close to the leaving ones.
  void dropClosing();
  /// How many screens at the bottom nothing pops: 1 with a root.
  [[nodiscard]] std::size_t base() const noexcept {
    return root_ ? 1 : 0;
  }
  /// The dock LB and RB step on `entry`: its own, else one in the chrome.
  [[nodiscard]] TabDock *dockFor(Entry &entry);
  /// The pointer on the chrome; true when a chrome widget took it.
  bool pointChrome(const PointerEvent &event);
  /// The pointer moved to `at` on `entry`: hovers and focuses what it is on.
  bool moveOn(Entry &entry, Point at);
  /// A tap or a click at `at`: the chrome's part there, else the top screen's widget.
  bool tap(Point at);
  /// A secondary press at `at`: the menu of what it is on.
  bool contextAt(Point at);
  /// `gesture` moved to `at`.
  bool dragTo(Gesture &gesture, Point at);
  /// Offers the drag to the widgets under its start, innermost first, then to the scroll views.
  void startDrag(Gesture &gesture, Entry &entry, Point at);
  /// The gesture ended at `at`.
  bool lift(Point at);
  /// Fires the long press of a gesture held still long enough.
  void checkHold();
  /// The speed the gesture moves at now, in pixels a second.
  [[nodiscard]] Point speed() const;
  void play(const std::function<void()> &sound) const;
  void paintEntry(Canvas &canvas, const Entry &entry, bool top);
  /// The lowest screen drawn: the top one, or below it while it is an overlay or still moving.
  [[nodiscard]] std::size_t firstDrawn() const;
  /// Draws the drawn screens and the leaving ones that are overlays when `overlays` is set, else
  /// those that are not.
  void paintLayer(Canvas &canvas, bool overlays);
  /// Shows the top screen's prompts on every open screen's hint bar.
  void placeHints();
  /// `entry`'s prompts: its focused widget's, then the screen's not already given.
  [[nodiscard]] std::vector<Hint> hintsOf(const Entry &entry) const;

  Typesetter &text_;
  Prompts &prompts_;
  Sounds sounds_;
  std::function<void()> keyboard_;
  std::function<std::string()> clipboard_;
  Theme theme_;
  double now_{};
  std::optional<Gesture> gesture_;
  Rect viewport_;
  float covered_{};
  Insets safe_;
  std::vector<Entry> stack_;
  /// Whether `stack_`'s first screen is the root.
  bool root_{false};
  bool touch_{false};
  std::vector<Chrome> chrome_;
  /// Popped screens fading out, drawn over the stack.
  std::vector<Entry> leaving_;
  /// How deep input handling is; stack changes wait until it is back to 0.
  int busy_{0};
  std::vector<Deferred> deferred_;
  /// A focus asked for while input was handled.
  std::optional<std::string> focusLater_;
  /// An edit started or stopped while input was handled or before the top screen was laid out.
  std::optional<bool> editLater_;
};

} // namespace gpu2d::ui
