# UI toolkit (`gpu2d::ui`)

A retained widget toolkit for menus and settings screens, drawn with `gpu2d::render`'s canvas. It
is title-neutral: the app supplies the theme's values, the face (`Typesetter`), the controller
glyphs (`Prompts`), the sounds and the input, and builds its screens from its own model.

## What it owns

| Concept | Where | Notes |
|---|---|---|
| Theme tokens | `theme.hpp` | Palette, spacing, shape (radii, outline, focus ring), type sizes, motion lengths; lengths in dp, `scale` pixels per dp |
| Input | `input.hpp` | `Action` (directions, Accept, Back, Secondary, Tertiary, tabs, Menu, Options for the focused thing's own menu, Search, and Erase and Submit for typing), `PointerEvent` (move, press, a secondary press `Context`, wheel, and `Leave` when the pointer stops driving the app), `Hint` |
| Text and glyphs | `typesetter.hpp`, `text_fit.hpp` | The app's face and prompts behind two interfaces (`Typesetter::drawCentred` puts a line's middle on a point, `sizeForLineHeight` inverts `lineHeight`; `Prompts::bound` says whether the device in use has a control for an action now, while typing or not); UTF-8-safe fitting with an ellipsis (`fittedEnd` keeps a line's end, `fittedAround` keeps a place such as a caret in view) and stepping (`characterStart`, `nextCharacter`) and word wrapping that keeps line breaks |
| Motion | `motion.hpp` | One clock (seconds the app sets per frame); `Tween` for screen fades and slides, `Follow`/`RectFollow` for scrolling and the focus ring |
| Widget tree | `widget.hpp` | `Widget` (stable id, measure, arrange, frame, paint, act, press, hints, `hover`, `wheel` and `context` for the pointer, `drawsFocus` for one that draws its own focus, and `takesText`/`type` for a widget typed into), `ViewState` (what outlives a rebuild: focus, the widget the pointer rests on, scrolls, the field being edited and its caret), `ActContext`, `Painter` (with `hovered`) |
| Layout | `layout.hpp` | `Column`, `Row`, `Stack`, `Grid`, `Card` (raised: drawn as a row, for a card on a card), `Backdrop`, `Centre` (a growing child takes its whole height), `Spacer`, `ScrollView`; padding and spacing in dp |
| Widgets | `widgets.hpp` | `Label`, `SectionHeading`, `Button`, `CardButton` (a card holding a picture and caption that A presses, ticked when marked), `Item` (title, subtitle, value, a source chip, X and Y actions, A that asks first; a value or choice that sized the row's end shows whole, cut only when the end is capped at its share of the row), `Toggle`, `Choice` (list words apart from the row's value), `Slider`, `Progress` (a bar filled to a fraction, or sweeping while work has no measure), `Tabs`, `List`, `TextField` (a captioned line that takes typed text at a caret Left and Right move while it is edited; masked for a secret, wiped when it goes; A asks the app for an on-screen keyboard; Y clears it, prompted "Clear" while it holds text), `HintBar`, `Toast`, `CellGrid` (a grid or rail of the app's cells, which the app moves, places and draws: one focusable widget whose cells hold the focus; the pointer focuses a cell, a press on one is A and on a page control turns the page, a secondary press asks for the cell's menu, the wheel steps), `TabDock` (tabs along a dock the app draws, never focused: LB and RB step them, a press chooses one, the pointer lights one) |
| Focus | `focus.hpp` | Spatial choice among boxes: nearest in a direction within a cone, overlap across the direction counting as straight ahead |
| Screens | `screen.hpp` | `Screen` (build, area, overlay, back, act, hints, initial focus), `Dialog` (its buttons each close it and do one thing; the one marked `exits` is what Back does) and `confirm` (the one rule for a step that throws something away), `ChoiceList` |
| Stack | `navigator.hpp` | `Navigator`: a root (`setRoot`, the app's home: never popped, `atRoot` says it is in front, `active` that a screen stands above it), push, pop, replace, clear, `closeFrom` (a screen and all above it); chrome screens outside the stack (`addChrome`, `paintChrome`); input, pointer, text, hints and drawing walk it from the top; `typing` says a text field holds the focus, `editing` that it is being edited and `edit` starts or stops that, and `setKeyboard` is the app's on-screen keyboard |

## Data flow

Each frame the app calls `setTheme` (when its scale or palette changed), `setTime`, then
`layout(viewport, covered)`: every open screen builds its tree from the app's model, measures
and arranges it once, and the navigator settles focus and scroll on it. Overlays lay out above the
bottom `covered` pixels a docked on-screen keyboard takes; full screens keep the whole viewport.
`paint(canvas)` then draws from the highest screen that covers the viewport upwards, reading the same frames that hit testing and
focus read; an app that draws its own layer between pages and overlays (a mapping editor, a
game) calls `paintPages` and `paintOverlays` around it instead, and `pagesVisible` says whether a
page above the root covers the window. Chrome screens (a top bar, a dock, a docked keyboard) are
built and arranged over the viewport after the stack and drawn by `paintChrome` wherever the app
puts them. Input (`act`, `pointer`, `text`) rebuilds the top screen first, so it acts on what the
model holds now; stack changes and focus moves asked for while input is handled are applied once
it is done, never under the widget handling it.

## Invariants and rules

- A widget's id is stable across rebuilds. Focus, scroll offsets and the child a group last
  focused are kept in the screen's `ViewState` by id; a focused widget that goes away hands the
  focus to the nearest one where it stood.
- One behaviour per widget kind: A presses a button or item and flips a toggle; Left and Right
  step a choice or slider; A opens a choice's options as a list; LB and RB step tabs from anywhere
  on the screen; focusing a tab chooses it. X and Y do a row's second and third actions when it
  names them. A horizontal tab strip wider than its frame scrolls the chosen tab into view; a tab
  label is never cut short.
- A step that throws something away asks first, one rule: `confirm` (the action focused: A, A does it; Back cancels); an item's
  `confirmFirst` puts it in front of its A.
- Back goes to the focused widget, then the screen (`back` may stay and handle it, e.g. to ask
  first), else pops the screen. A press outside an overlay's card is Back.
- Moving focus into a group lands on its preferred child, else the one it last focused, else its
  first visible child. Up or Down with nothing further to focus scrolls the view holding the focus
  a row.
- Scrolling eases the focus into view with the section spacing as margin; the first focusable
  reveals the top, the last the end. Focus the pointer moved scrolls nothing.
- Every hint bar shows the top screen's prompts: the focused widget's, then the screen's ones not
  already named, less those naming an action the app's `Prompts::bound` says the device cannot do
  now; Back stands first. A bar too narrow for all of them drops the last ones. A press
  on a prompt performs its action.
- Typed text goes to the focused widget's `type`, then the screen. A text field is being edited
  from A, typed text or Erase (or the app's `Navigator::edit(true)`) until the focus leaves it,
  Submit or `edit(false)`; editing starts with the caret at the end. While edited, Left and Right
  move the caret one UTF-8 character (taken even at either end, so the focus stays), typed text
  goes in at the caret and Erase removes the character before it; Up and Down still leave the
  field. Not edited, Left and Right move the focus as anywhere else. Tertiary empties it, Submit
  does its `onSubmit`; it refuses text past its limit whole. The caret and the field being edited
  live in the screen's `ViewState`, so they outlive the rebuild each change makes; the line shows
  the stretch around the caret (`fittedAround`: its end, else its start, else what ends at the
  caret between ellipses). The app owns the keyboard: A on a field calls the `setKeyboard` hook,
  and the app keeps its keyboard up while `editing` holds.
- The pointer reaches the chrome first, the last added first, wherever one of its widgets is; a
  press there acts on the stack at once. Elsewhere it reaches the top screen: a move focuses the
  focusable widget under it and calls its `hover`, lighting it in `ViewState::hovered`; a press
  focuses and presses it; a secondary press focuses it and calls its `context`, else it is the
  focused widget's Secondary; the wheel scrolls the scroll view under it, else goes to the focused
  widget's `wheel`; `Leave` clears what is lit.
- The root stays: Back on it goes to its screen's `back` and never pops it, and `pop`, `clear` and
  `closeFrom` leave it.
- LB and RB step the top screen's `Tabs`, else its `TabDock`, else a `TabDock` in the chrome.
- Sounds are the app's callbacks for a focus move, an activation and Back.

## Checking it

`ctest --test-dir build/agent -R ui_` runs the layout, navigator, widget, cell grid and tab dock
tests against fake
text and prompts (no GPU). In an app, drive the screens through its own input path and read the
navigator's `focused()` and `screens()`; openSU's control channel reports them as `screen` and
`screenFocus` and takes `POST /pointer`.
