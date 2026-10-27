#pragma once
// Rocker mapping and focus model (docs/ui/layout.md section 7). Pure logic.
#include <vector>

#include "ui_layout.h"

namespace ui {

enum class PhysicalKey { Up, Down, Push, LongPush };
enum class LogicalKey { Prev, Next, Push, LongPush };

// UP -> PREV, DOWN -> NEXT in both orientations; invert swaps PREV and NEXT.
LogicalKey mapRocker(PhysicalKey key, settings::Orientation o, bool invert);

enum class FocusMode { Tab, Widget, Inner };

struct FocusState {
  FocusMode mode = FocusMode::Tab;
  int tab = 0;
  int group = 0;
  int subPage = 0;
  int stop = 0;   // WIDGET/INNER: 1-based focus stop on subPage (0 = tab bar)
  int inner = 0;  // INNER: sub-region id of the active button
  bool ringVisible = false;  // false until the first rocker event after wake
};

// Everything the focus model needs to know about the active tab.
struct FocusEnv {
  settings::Orientation orientation = settings::Orientation::Portrait;
  int tabCount = 0;
  const Flow *flow = nullptr;                  // flow of state.tab
  std::vector<bool> unavailable;               // by widget index; missing = available
};

bool isFocusable(config::WidgetType t);
bool isSingleAction(config::WidgetType t);  // light, switch, automation
// Focusable, available widgets of a sub-page sorted by (row, col); widget indices.
std::vector<int> focusStops(const FocusEnv &env, int subPage);
// Sub-region ids in INNER order for the widget (empty for single-action widgets).
std::vector<int> innerStops(const FocusEnv &env, int widgetIndex);

enum class Effect {
  None,
  FocusMoved,       // ring moved (partial redraw of old/new rect)
  TabChanged,       // new tab: subPage reset, full refresh
  SubPageChanged,   // full refresh
  OpenSettings,     // LONG_PUSH in TAB mode: open the Settings screen (no ring change)
  WidgetAction,     // run action of widgetIndex / sub (0 = single action)
};

struct FocusResult {
  Effect effect = Effect::None;
  int widgetIndex = -1;
  int sub = 0;
};

FocusResult focusHandle(FocusState &st, const FocusEnv &env, LogicalKey key);

// Widget index of the focused widget in WIDGET/INNER mode, else -1.
int focusedWidget(const FocusState &st, const FocusEnv &env);

// Touch interplay.
void focusTouchTab(FocusState &st, settings::Orientation o, int tabCount, int tab);
void focusOverflow(FocusState &st, settings::Orientation o, int tabCount);
// Touch pager: no wrap. Returns true if the sub-page changed (focus returns to TAB mode).
bool focusPagerStep(FocusState &st, int delta, int pageCount);

}  // namespace ui
