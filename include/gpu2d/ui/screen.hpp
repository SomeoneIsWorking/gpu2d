// screen — one entry of the navigator's stack: it builds its widget tree, says how it lays out in
// the viewport, and answers what no focused widget took (Back, the screen's own actions, typed
// text). Two screens come with the toolkit: `Dialog`, the one rule for confirming (a card over the
// dimmed screen whose buttons each do one thing), and `ChoiceList`, a Choice's options as a list.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gpu2d/ui/widget.hpp"
#include "gpu2d/ui/widgets.hpp"

namespace gpu2d::ui {

class Screen {
public:
  enum class BackReply : std::uint8_t {
    /// Close the screen.
    Pop,
    /// The screen handled Back itself.
    Stay,
  };

  Screen(std::string id, std::string title) : id_{std::move(id)}, title_{std::move(title)} {
  }
  virtual ~Screen() = default;
  Screen(const Screen &) = delete;
  Screen &operator=(const Screen &) = delete;
  Screen(Screen &&) = delete;
  Screen &operator=(Screen &&) = delete;

  [[nodiscard]] const std::string &id() const noexcept {
    return id_;
  }
  [[nodiscard]] virtual std::string title() const {
    return title_;
  }

  /// The tree to show now, in `theme`'s sizes. Called before every layout and before input
  /// reaches the screen.
  [[nodiscard]] virtual std::unique_ptr<Widget> build(const Theme &theme) = 0;
  /// Where the tree lays out in the navigator's viewport.
  [[nodiscard]] virtual Rect area(const Rect &viewport, const Theme &theme) const;
  /// Whether the screens under it still show (a dialog, a list over its row).
  [[nodiscard]] virtual bool overlay() const {
    return false;
  }
  /// Drawn over the screens below and under the tree: an overlay's scrim.
  virtual void paintBackdrop(Painter &painter, const Rect &viewport) const;

  /// Back that no focused widget took.
  virtual BackReply back(ActContext & /*context*/) {
    return BackReply::Pop;
  }
  /// An action no focused widget took; true when handled.
  virtual bool act(Action /*action*/, ActContext & /*context*/) {
    return false;
  }
  /// Typed text; true when taken.
  virtual bool text(std::string_view /*typed*/, ActContext & /*context*/) {
    return false;
  }
  /// The prompts it adds after the focused widget's.
  [[nodiscard]] virtual std::vector<Hint> hints() const;
  /// The widget focused when it first shows; empty for the first focusable.
  [[nodiscard]] virtual std::string initialFocus() const {
    return {};
  }

  /// Asks the navigator to pop it once the input being handled is done.
  void close() noexcept {
    closing_ = true;
  }
  [[nodiscard]] bool closing() const noexcept {
    return closing_;
  }

  [[nodiscard]] ViewState &view() noexcept {
    return view_;
  }
  [[nodiscard]] const ViewState &view() const noexcept {
    return view_;
  }

private:
  std::string id_;
  std::string title_;
  ViewState view_;
  bool closing_{false};
};

/// A card over the dimmed screen: a title, a paragraph and buttons, each closing it and doing its
/// one thing. Back is the button marked as the way out, or just closes it.
class Dialog : public Screen {
public:
  struct Choice {
    std::string label;
    std::function<void()> chosen;
    ButtonKind kind{ButtonKind::Normal};
    /// The way out: Back does what this button does.
    bool exits{false};
  };

  Dialog(std::string title, std::string body, std::vector<Choice> choices, std::size_t focused = 0)
      : Screen{"dialog", std::move(title)}, body_{std::move(body)}, choices_{std::move(choices)},
        focused_{focused} {
  }

  /// The paragraph under the title.
  [[nodiscard]] const std::string &body() const noexcept {
    return body_;
  }
  [[nodiscard]] std::unique_ptr<Widget> build(const Theme &theme) override;
  [[nodiscard]] Rect area(const Rect &viewport, const Theme &theme) const override;
  [[nodiscard]] bool overlay() const override {
    return true;
  }
  void paintBackdrop(Painter &painter, const Rect &viewport) const override;
  [[nodiscard]] BackReply back(ActContext &context) override;
  [[nodiscard]] std::vector<Hint> hints() const override;
  [[nodiscard]] std::string initialFocus() const override;

private:
  std::string body_;
  std::vector<Choice> choices_;
  std::size_t focused_;
};

/// The one rule for a step that throws something away or cannot be undone: a dialog naming it,
/// with its action focused, so A, A does the step and Back or Cancel keeps things as they are.
[[nodiscard]] std::unique_ptr<Dialog> confirm(std::string title, std::string body,
                                              std::string action, std::function<void()> confirmed);

/// A Choice's options as a list over the screen; A picks one, Back keeps what was chosen.
class ChoiceList : public Screen {
public:
  ChoiceList(std::string title, std::vector<std::string> options, std::size_t chosen,
             std::function<void(std::size_t)> picked)
      : Screen{"choice", std::move(title)}, options_{std::move(options)}, chosen_{chosen},
        picked_{std::move(picked)} {
  }

  [[nodiscard]] std::unique_ptr<Widget> build(const Theme &theme) override;
  [[nodiscard]] Rect area(const Rect &viewport, const Theme &theme) const override;
  [[nodiscard]] bool overlay() const override {
    return true;
  }
  void paintBackdrop(Painter &painter, const Rect &viewport) const override;
  [[nodiscard]] std::vector<Hint> hints() const override;
  [[nodiscard]] std::string initialFocus() const override;

private:
  std::vector<std::string> options_;
  std::size_t chosen_;
  std::function<void(std::size_t)> picked_;
};

} // namespace gpu2d::ui
