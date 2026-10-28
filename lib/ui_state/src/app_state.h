#pragma once
// Application state of the tab screen: config, entity states, what is shown, rocker focus, and
// pending (fake) actions. Pure logic driven by events with a millisecond clock; the firmware only
// renders it (docs/ui/layout.md section 7, widgets.md).
#include <cstdint>
#include <functional>
#include <map>
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

// Result of a driver action for an entity (real mode). None = no mark.
enum class Mark { None, Pending, Failed };

struct Pending {
  int tab = 0;
  int widget = 0;
  int sub = 0;
  uint32_t commitAt = 0;
  std::vector<EntityState> states;  // requested result, committed to the store when due
  bool seenMark = false;            // marks-driven mode: the driver reported Pending for it
};

// Called when a tap starts an action: the widget's tab/index, the sub-region and the requested
// result (what the entities should look like). Real mode sends it to Home Assistant.
using ActionSink = std::function<void(int tab, int widget, int sub, const std::vector<EntityState> &requested)>;

// What a widget is drawn from.
struct WidgetData {
  config::WidgetType type = config::WidgetType::Sensor;
  bool group = false;
  std::vector<const EntityState *> members;
  bool pending = false;
  int pendingSub = 0;
  bool failed = false;  // the last action on this widget failed: real state plus alert badge
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

  void setActionSink(ActionSink s) { sink_ = std::move(s); }
  // Real mode: pending lasts until the driver's marks say otherwise (no local commit; a safety
  // timeout drops an entry the driver never marked). Off (default) = demo timer.
  void setMarksDriven(bool on, uint32_t safetyMs = 30000) { marksDriven_ = on; safetyMs_ = safetyMs; }
  // The driver's marks by entity id (absent = confirmed/none). Pending entries whose entities
  // were marked Pending and no longer are get dropped (confirmed); Failed ones are dropped and the
  // widget shows the failed state until the mark clears. Returns true if something visible changed.
  bool setMarks(const std::map<std::string, Mark> &marks);

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
  ActionSink sink_;
  bool marksDriven_ = false;
  uint32_t safetyMs_ = 30000;
  std::map<std::string, Mark> marks_;
};

}  // namespace uistate
