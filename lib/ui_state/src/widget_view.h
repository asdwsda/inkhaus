#pragma once
// Widget view models: what each widget type shows for given entity states (docs/ui/widgets.md).
// Pure text/flag logic, no drawing and no Arduino. Strings are UTF-8; the degree sign is
// "\xC2\xB0" (the built-in fonts have no glyph for it, the renderer draws it).
#include <string>
#include <vector>

#include "config_model.h"
#include "entity_state.h"

namespace uistate {

constexpr const char *kDeg = "\xC2\xB0";

enum class Look { Normal, Loading, Unavailable };

enum class ActionIcon { None, Flame, Snow, Dry, Fan, Pause, Auto, Power };
enum class WeatherIcon { Sunny, ClearNight, PartlyCloudy, Cloudy, Rainy, Pouring, Snowy, Fog, Windy,
                         Lightning, Unknown };

// "21.5", "-12.5", "22" (decimals 0). No "-0".
std::string fmtNumber(double v, int decimals);

// Widget title: config name, else the first entity's friendly_name, else its id.
std::string widgetName(const config::Widget &w, const EntityState *first);

struct CoverView {
  Look look = Look::Loading;
  std::string state = "-";
  bool upFilled = false, downFilled = false;
  bool alertBadge = false;  // group with some (not all) members unavailable
};
// members: one entry per entity (nullptr = no cached value). group = aggregation of section 2.
CoverView coverView(const std::vector<const EntityState *> &members, bool group);

// Icon of a toggle widget's leading glyph (widgets.md 11.1 pairs). The renderer maps it to Icon.
enum class StateIcon { Default, WindowOn, WindowOff, DoorOn, DoorOff, GarageOn, GarageOff, MotionOn, MotionOff,
                       PersonOn, PersonOff, PuddleOn, PuddleOff, SmokeOn, SmokeOff, GasOn, GasOff, ProblemOn,
                       ProblemOff, BatteryLow, Battery, Wifi, WifiOff, LockOpen, LockClosed, Plug, PlugOff, Bolt,
                       BoltOff, DotOn, DotOff };

struct ToggleView {
  Look look = Look::Loading;
  bool on = false;
  std::string state = "-";
  StateIcon icon = StateIcon::Default;  // Default: the renderer's per-type icon (light, switch, automation)
};
// type: Light, Switch, Automation or BinarySensor. badgeShown: a badge occupies the line end, so
// the brightness is left out.
// Representative member of a toggle group: first available "on" member, else first available,
// else members[0] (nullptr if empty). Feed it to toggleView so a mixed group reads On.
const EntityState *toggleSummary(const std::vector<const EntityState *> &members);
ToggleView toggleView(config::WidgetType type, const EntityState *st, bool badgeShown);

struct ClimateView {
  Look look = Look::Loading;
  std::string temp = "-";    // "21.5°" or "-" / "n/a"
  std::string hum;           // "45%", empty if unknown
  std::string target = "-";  // "22.0°", "Off", "n/a", "-"
  ActionIcon act = ActionIcon::None;
  bool off = false;
  bool targetKnown = false;
};
ClimateView climateView(const EntityState *st);

struct SensorView {
  Look look = Look::Loading;
  std::string value = "-";  // raw numeric text or a text state
  std::string unit;         // with a leading space where one is needed
  bool numeric = false;
  bool textState = false;  // draw in F24 in every size
};
// Generic fallback (widgets.md 11.4): unknown/unavailable (any case) -> "n/a"; empty -> "-"; else
// underscores to spaces, lower-cased, first letter upper-cased ("HEAT_COOL" -> "Heat cool").
std::string humanise(const std::string &raw);
SensorView sensorView(const EntityState *st);

struct WeatherView {
  Look look = Look::Loading;
  std::string temp = "-";  // integer, e.g. "12"; "n/a" / "-"
  std::string unit;        // "°C"
  std::string cond;        // short condition text
  std::string hum;         // "76%" or empty
  WeatherIcon icon = WeatherIcon::Unknown;
};
WeatherView weatherView(const EntityState *st);

struct VacuumView {
  Look look = Look::Loading;
  std::string status = "-";
  bool error = false;
  bool showBattery = false;
  int battery = 0;
  bool lowBattery = false;  // <= 20 % and not docked (widgets.md 11.2): battery icon gets the "!"
  bool startFilled = false, pauseFilled = false, dockFilled = false;
};
VacuumView vacuumView(const EntityState *st);

}  // namespace uistate
