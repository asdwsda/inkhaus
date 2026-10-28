#pragma once
// Application state of the tab screen: config, entity states, what is shown, rocker focus, and
// pending (fake) actions. Pure logic driven by events with a millisecond clock; the firmware only
// renders it (docs/ui/layout.md section 7, widgets.md).
#include <cstdint>
#include <string>
#include <vector>

#include "config_model.h"
#include "entity_state.h"
#include "settings.h"
#include "state_store.h"
#include "ui_layout.h"
#include "ui_rocker.h"

namespace uistate {

constexpr uint32_t kPendingMs = 600;        // demo: how long a fake action stays pending
constexpr uint32_t kTabDebounceMs = 400;    // rocker tab switch: content follows after this

struct Pending {
  int tab = 0;
  int widget = 0;
  int sub = 0;
  uint32_t commitAt = 0;
  std::vector<EntityState> states;  // requested result, committed to the store when due
};

// What a widget is drawn from.
struct WidgetData {
  config::WidgetType type = config::WidgetType::Sensor;
  bool group = false;
  std::vector<const EntityState *> members;
  bool pending = false;
  int pendingSub = 0;
};

enum class Screen { Tabs, ConfigError };

class AppState {
 public:
  // Takes a valid config. `demo` shows the "Demo" label.
  void init(const config::Config &cfg, settings::Orientation o, bool demo);
  // Error screen state: the first error is shown.
  void initError(const std::vector<config::ConfigError> &errors, settings::Orientation o);

  // Events. All return true if something visible changed.
  bool touch(ui::Point p, uint32_t now);
  bool rocker(ui::PhysicalKey key, uint32_t now);
  bool tick(uint32_t now);  // commits due pending actions, applies the debounced tab switch

  // The content (not only chrome) changed in a way that needs a full-quality refresh (tab or
  // sub-page change). Cleared by takeFull().
  bool takeFull();

  // For tests and the demo: change pending duration (default kPendingMs).
  void setPendingMs(uint32_t ms) { pendingMs_ = ms; }

  // ---- view ----
  Screen screen() const { return screen_; }
  bool demo() const { return demo_; }
  settings::Orientation orientation() const { return orientation_; }
  const config::Config &config() const { return cfg_; }
  const std::vector<config::ConfigError> &errors() const { return errors_; }
  const ui::Layout &layout() const { return layout_; }
  const ui::FocusState &focus() const { return focus_; }
  int shownTab() const { return shownTab_; }
  StateStore &store() { return store_; }
  const StateStore &store() const { return store_; }
  std::string widgetTitle(int widgetIndex) const;
  WidgetData widgetData(int widgetIndex) const;
  // Widget index in the shown tab with the focus ring, or -1.
  int focusedWidgetIndex() const;
  bool pendingAny() const { return !pending_.empty(); }
  const std::vector<Pending> &pendingList() const { return pending_; }

 private:
  void relayout();
  void refreshFocusFlow();
  ui::FocusEnv focusEnv() const;
  bool applyTabSwitch();
  bool runAction(int widgetIndex, int sub, uint32_t now);
  const config::Widget *widgetAt(int tab, int idx) const;
  std::vector<const EntityState *> membersOf(const config::Widget &w) const;

  Screen screen_ = Screen::Tabs;
  bool demo_ = false;
  settings::Orientation orientation_ = settings::Orientation::Portrait;
  config::Config cfg_;
  std::vector<config::ConfigError> errors_;
  StateStore store_;
  ui::Layout layout_;
  ui::FocusState focus_;
  ui::Flow focusFlow_;
  int shownTab_ = 0, shownSubPage_ = 0;
  bool switchDue_ = false;
  uint32_t switchAt_ = 0;
  bool full_ = false;
  uint32_t pendingMs_ = kPendingMs;
  std::vector<Pending> pending_;
};

}  // namespace uistate
