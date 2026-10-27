#include "gpu2d/ui/screen.hpp"

#include <algorithm>

#include "gpu2d/ui/layout.hpp"

namespace gpu2d::ui {
namespace {

/// How much of the viewport's height an option list may take.
constexpr float listShare = 0.7f;

void paintScrim(Painter &painter, const Rect &viewport) {
  painter.canvas.fillRect(viewport, painter.fade(painter.theme.colours.scrim));
}

} // namespace

Rect Screen::area(const Rect &viewport, const Theme & /*theme*/) const {
  return viewport;
}

void Screen::paintBackdrop(Painter & /*painter*/, const Rect & /*viewport*/) const {
}

std::vector<Hint> Screen::hints() const {
  return {Hint{{Action::Back}, "Back"}};
}

std::unique_ptr<Widget> Dialog::build(const Theme &theme) {
  const Spacing &spacing = theme.spacing;
  auto root = std::make_unique<Centre>(spacing.dialogWidth);
  root->pad(Insets::all(spacing.pad));
  auto &card = root->add<Card>();
  card.pad(Insets::symmetric(spacing.pad * 1.5f, spacing.pad * 1.4f)).space(spacing.pad);
  card.add<Label>(title(), TextRole::Title, true);
  if (!body_.empty()) {
    card.add<Label>(body_, TextRole::Body, true);
  }
  auto &buttons = card.add<Row>();
  buttons.space(spacing.gap * 2.0f);
  buttons.add<Spacer>();
  for (std::size_t i = 0; i < choices_.size(); ++i) {
    const Choice &choice = choices_[i];
    buttons.add<Button>(
        "dialog/" + std::to_string(i), choice.label,
        [this, i] {
          close();
          if (choices_[i].chosen) {
            choices_[i].chosen();
          }
        },
        choice.kind);
  }
  return root;
}

Rect Dialog::area(const Rect &viewport, const Theme & /*theme*/) const {
  return viewport;
}

void Dialog::paintBackdrop(Painter &painter, const Rect &viewport) const {
  paintScrim(painter, viewport);
}

std::vector<Hint> Dialog::hints() const {
  return {Hint{{Action::Back}, "Cancel"}};
}

std::string Dialog::initialFocus() const {
  return "dialog/" + std::to_string(std::min(focused_, choices_.empty() ? 0 : choices_.size() - 1));
}

std::unique_ptr<Dialog> confirm(std::string title, std::string body, std::string action,
                                std::function<void()> confirmed) {
  std::vector<Dialog::Choice> choices{
      Dialog::Choice{"Cancel", {}, ButtonKind::Normal},
      Dialog::Choice{std::move(action), std::move(confirmed), ButtonKind::Danger}};
  return std::make_unique<Dialog>(std::move(title), std::move(body), std::move(choices), 0);
}

std::unique_ptr<Widget> ChoiceList::build(const Theme &theme) {
  const Spacing &spacing = theme.spacing;
  auto root = std::make_unique<Centre>(spacing.dialogWidth);
  root->pad(Insets::all(spacing.pad));
  auto &card = root->add<Card>();
  card.pad(Insets::all(spacing.pad)).space(spacing.gap * 2.0f);
  card.add<Label>(title(), TextRole::Title);
  auto &list = card.add<List>("choice/list", spacing.gap);
  list.grow();
  for (std::size_t i = 0; i < options_.size(); ++i) {
    list.rows()
        .add<Item>("choice/" + std::to_string(i), Item::Text{options_[i], {}})
        .onPress(
            [this, i] {
              close();
              if (picked_ && i != chosen_) {
                picked_(i);
              }
            },
            "Choose")
        .mark(i == chosen_);
  }
  return root;
}

Rect ChoiceList::area(const Rect &viewport, const Theme &theme) const {
  // Tall lists stop short of the viewport's edges.
  const float height =
      std::min(viewport.height, viewport.height * listShare + theme.px(theme.spacing.pad) * 2.0f);
  return Rect{viewport.x, viewport.y + (viewport.height - height) * 0.5f, viewport.width, height};
}

void ChoiceList::paintBackdrop(Painter &painter, const Rect &viewport) const {
  paintScrim(painter, viewport);
}

std::vector<Hint> ChoiceList::hints() const {
  return {Hint{{Action::Back}, "Cancel"}};
}

std::string ChoiceList::initialFocus() const {
  return "choice/" + std::to_string(chosen_);
}

} // namespace gpu2d::ui
