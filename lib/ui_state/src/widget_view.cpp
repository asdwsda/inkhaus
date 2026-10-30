#include "widget_view.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace uistate {

namespace {

std::string pct(int v) { return std::to_string(v) + "%"; }

std::string capitalize(std::string s) {
  if (!s.empty() && s[0] >= 'a' && s[0] <= 'z') s[0] = char(s[0] - 'a' + 'A');
  return s;
}

bool parseNumber(const std::string &s, double &out) {
  // Finite decimal numbers only: [+-] digits [. digits] [e|E [+-] digits]. No nan, inf, hex, spaces.
  size_t i = 0;
  const size_t n = s.size();
  if (i < n && (s[i] == '+' || s[i] == '-')) ++i;
  size_t digits = 0;
  while (i < n && s[i] >= '0' && s[i] <= '9') { ++i; ++digits; }
  if (i < n && s[i] == '.') {
    ++i;
    while (i < n && s[i] >= '0' && s[i] <= '9') { ++i; ++digits; }
  }
  if (digits == 0) return false;
  if (i < n && (s[i] == 'e' || s[i] == 'E')) {
    ++i;
    if (i < n && (s[i] == '+' || s[i] == '-')) ++i;
    size_t ed = 0;
    while (i < n && s[i] >= '0' && s[i] <= '9') { ++i; ++ed; }
    if (ed == 0) return false;
  }
  if (i != n) return false;
  const double v = std::strtod(s.c_str(), nullptr);
  if (!std::isfinite(v)) return false;
  out = v;
  return true;
}

Look lookOf(const EntityState *st) {
  if (!st) return Look::Loading;
  return st->available ? Look::Normal : Look::Unavailable;
}

// widgets.md 11.1: texts and icon pair by device_class (on text / off text, icon on / off).
void binaryTexts(const std::string &dc, ToggleView &v) {
  struct Row { const char *dc, *onText, *offText; StateIcon on, off; };
  using I = StateIcon;
  static const Row rows[] = {
      {"door", "Open", "Closed", I::DoorOn, I::DoorOff},
      {"garage_door", "Open", "Closed", I::GarageOn, I::GarageOff},
      {"window", "Open", "Closed", I::WindowOn, I::WindowOff},
      {"opening", "Open", "Closed", I::WindowOn, I::WindowOff},
      {"motion", "Motion", "Clear", I::MotionOn, I::MotionOff},
      {"occupancy", "Present", "Empty", I::PersonOn, I::PersonOff},
      {"presence", "Home", "Away", I::PersonOn, I::PersonOff},
      {"moisture", "Wet", "Dry", I::PuddleOn, I::PuddleOff},
      {"smoke", "Smoke", "Clear", I::SmokeOn, I::SmokeOff},
      {"gas", "Gas", "Clear", I::GasOn, I::GasOff},
      {"problem", "Problem", "OK", I::ProblemOn, I::ProblemOff},
      {"battery", "Low", "OK", I::BatteryLow, I::Battery},
      {"connectivity", "Online", "Offline", I::Wifi, I::WifiOff},
      {"lock", "Open", "Locked", I::LockOpen, I::LockClosed},
      {"plug", "Plugged", "Out", I::Plug, I::PlugOff},
      {"power", "Power", "None", I::Bolt, I::BoltOff},
  };
  for (const Row &r : rows) {
    if (dc == r.dc) {
      v.state = v.on ? r.onText : r.offText;
      v.icon = v.on ? r.on : r.off;
      return;
    }
  }
  v.icon = v.on ? I::DotOn : I::DotOff;
}

}  // namespace

std::string fmtNumber(double v, int decimals) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
  std::string s = buf;
  if (s == "-0" || s == "-0.0" || s == "-0.00") s.erase(0, 1);
  return s;
}

std::string widgetName(const config::Widget &w, const EntityState *first) {
  if (!w.name.empty()) return w.name;
  if (first && !first->friendly_name.empty()) return first->friendly_name;
  if (!w.entities.empty()) return w.entities[0];
  return w.entity;
}

CoverView coverView(const std::vector<const EntityState *> &members, bool group) {
  CoverView v;
  size_t present = 0, avail = 0;
  for (auto *m : members) {
    if (m) ++present;
    if (m && m->available) ++avail;
  }
  if (present == 0) return v;  // loading
  if (avail == 0) {
    v.look = Look::Unavailable;
    v.state = "n/a";
    return v;
  }
  v.look = Look::Normal;
  v.alertBadge = group && avail < members.size();

  bool anyOpening = false, anyClosing = false, allClosed = true, samePos = true;
  std::optional<int> pos;
  bool firstPos = true;
  for (auto *m : members) {
    if (!m || !m->available) continue;
    anyOpening |= m->state == "opening";
    anyClosing |= m->state == "closing";
    allClosed &= m->state == "closed";
    if (firstPos) {
      pos = m->current_position;
      firstPos = false;
    } else if (pos != m->current_position) {
      samePos = false;
    }
  }
  v.upFilled = anyOpening;
  v.downFilled = anyClosing;

  auto withPos = [&](const char *word, std::optional<int> p) {
    std::string s = word;
    if (p) s += " " + pct(*p);
    return s;
  };
  if (!group) {
    const EntityState *m = nullptr;
    for (auto *x : members) if (x) { m = x; break; }
    if (m->state == "unknown") v.state = "-";
    else if (m->state == "unavailable") {
      v.look = Look::Unavailable;
      v.state = "n/a";
    } else if (m->state == "stopped") v.state = "Stopped";
    else if (m->state == "closed") v.state = "Closed";
    else if (m->state == "open") v.state = (m->current_position && *m->current_position < 100) ? withPos("Open", m->current_position) : "Open";
    else if (m->state == "opening") v.state = withPos("Opening", m->current_position);
    else if (m->state == "closing") v.state = withPos("Closing", m->current_position);
    else v.state = humanise(m->state);
    return v;
  }
  bool allUnknown = true;
  for (auto *m : members) if (m && m->available) allUnknown &= m->state == "unknown";
  if (allUnknown) v.state = "-";
  else if (anyOpening) v.state = "Opening";
  else if (anyClosing) v.state = "Closing";
  else if (allClosed) v.state = "Closed";
  else if (samePos && pos) v.state = (*pos < 100) ? withPos("Open", pos) : "Open";
  else v.state = "Mixed";
  return v;
}

const EntityState *toggleSummary(const std::vector<const EntityState *> &members) {
  // Group is "on" if any available member is on (same rule as toggleIntent).
  const EntityState *firstAvail = nullptr;
  for (auto *m : members) {
    if (!m || !m->available) continue;
    if (m->state == "on") return m;
    if (!firstAvail) firstAvail = m;
  }
  if (firstAvail) return firstAvail;
  return members.empty() ? nullptr : members[0];
}

ToggleView toggleView(config::WidgetType type, const EntityState *st, bool badgeShown) {
  ToggleView v;
  v.look = lookOf(st);
  if (v.look == Look::Loading) return v;
  if (v.look == Look::Unavailable) {
    v.state = "n/a";
    return v;
  }
  v.on = st->state == "on";
  v.state = v.on ? "On" : "Off";
  if (st->state == "unknown") {
    v.state = "-";
    return v;
  }
  if (type == config::WidgetType::BinarySensor) {
    binaryTexts(st->device_class, v);
  }
  if (v.on && type == config::WidgetType::Light && st->brightness && !badgeShown) {
    v.state += " " + pct(int(std::lround(*st->brightness * 100.0 / 255.0)));
  }
  return v;
}

ClimateView climateView(const EntityState *st) {
  ClimateView v;
  v.look = lookOf(st);
  if (v.look == Look::Loading) return v;
  if (v.look == Look::Unavailable) {
    v.temp = "n/a";
    v.target = "n/a";
    return v;
  }
  if (st->current_temperature) v.temp = fmtNumber(*st->current_temperature, 1) + kDeg;
  else v.temp = "-";
  if (st->current_humidity) v.hum = pct(int(std::lround(*st->current_humidity)));
  const std::string mode = st->hvac_mode.empty() ? st->state : st->hvac_mode;
  v.off = mode == "off";
  const double step = st->target_temp_step.value_or(0.5);
  if (v.off) {
    v.target = "Off";
    v.targetKnown = st->temperature.has_value();
  } else if (st->temperature) {
    v.target = fmtNumber(*st->temperature, step < 1.0 ? 1 : 0) + kDeg;
    v.targetKnown = true;
  }
  const std::string &a = st->hvac_action;
  if (a == "heating") v.act = ActionIcon::Flame;
  else if (a == "cooling") v.act = ActionIcon::Snow;
  else if (a == "drying") v.act = ActionIcon::Dry;
  else if (a == "fan") v.act = ActionIcon::Fan;
  else if (a == "idle") v.act = ActionIcon::Pause;
  else if (mode == "heat") v.act = ActionIcon::Flame;
  else if (mode == "cool") v.act = ActionIcon::Snow;
  else if (mode == "heat_cool" || mode == "auto") v.act = ActionIcon::Auto;
  else if (mode == "off") v.act = ActionIcon::Power;
  else if (mode == "fan_only") v.act = ActionIcon::Fan;
  else if (mode == "dry") v.act = ActionIcon::Dry;
  return v;
}

std::string humanise(const std::string &raw) {
  std::string s = raw;
  for (char &c : s) {
    if (c == '_') c = ' ';
    else if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  }
  if (s == "unavailable") return "n/a";
  if (s == "unknown") return "-";
  if (s.empty()) return "-";
  return capitalize(s);
}

namespace {

std::string unitText(const std::string &u) {
  if (u.empty()) return u;
  const bool tight = u == "\xC2\xB0" "C" || u == "\xC2\xB0" "F" || u == "%";
  return (tight ? "" : " ") + u;
}

// Numeric text: explicit decimals round; otherwise integers stay as written and anything with a
// fraction (or exponent) gets 1 decimal ("45.37" -> "45.4", raw HA states can carry 13 digits).
std::string numText(const std::string &raw, double d, const std::optional<int> &decimals) {
  if (decimals) return fmtNumber(d, *decimals);
  if (raw.find_first_of(".eE") == std::string::npos) return raw;
  return fmtNumber(d, 1);
}

// Value text of one attribute: numbers rounded when decimals is set, text humanised, missing "-".
std::string attrValue(const std::string *raw, const std::optional<int> &decimals, bool &numeric) {
  numeric = false;
  if (!raw) return "-";
  double d;
  if (parseNumber(*raw, d)) {
    numeric = true;
    return numText(*raw, d, decimals);
  }
  return humanise(*raw);
}

}  // namespace

SensorView sensorView(const EntityState *st, const config::Widget *opts) {
  SensorView v;
  v.look = lookOf(st);
  if (v.look == Look::Loading) return v;
  if (v.look == Look::Unavailable) {
    v.value = "n/a";
    return v;
  }
  const bool useAttr = opts && !opts->attribute.empty();
  const std::optional<int> decimals = opts ? opts->decimals : std::nullopt;
  if (useAttr) {
    v.value = attrValue(st->attr(opts->attribute), decimals, v.numeric);
  } else {
    double d;
    v.numeric = parseNumber(st->state, d);
    v.value = v.numeric ? numText(st->state, d, decimals) : humanise(st->state);
  }
  v.textState = !v.numeric;
  if (v.numeric) {
    if (opts && opts->unit) {
      v.unit = unitText(*opts->unit);
    } else if (useAttr) {
      if (const std::string *u = st->attr(opts->attribute + "_unit")) v.unit = unitText(*u);
    } else {
      v.unit = unitText(st->unit_of_measurement);
    }
  }
  if (opts)
    for (const std::string &name : opts->details) {
      DetailItem item;
      item.label = humanise(name);
      bool numeric = false;
      item.value = attrValue(st->attr(name), decimals, numeric);
      if (numeric)
        if (const std::string *u = st->attr(name + "_unit")) item.unit = unitText(*u);
      v.details.push_back(item);
    }
  return v;
}

WeatherView weatherView(const EntityState *st) {
  WeatherView v;
  v.look = lookOf(st);
  if (v.look == Look::Loading) return v;
  if (v.look == Look::Unavailable) {
    v.temp = "n/a";
    return v;
  }
  if (st->temperature) v.temp = fmtNumber(*st->temperature, 0);
  v.unit = st->temperature_unit.empty() ? std::string(kDeg) + "C" : st->temperature_unit;
  const std::string &s = st->state;
  struct Row { const char *state; const char *text; WeatherIcon icon; };
  static const Row rows[] = {
      {"sunny", "Sunny", WeatherIcon::Sunny},
      {"clear-night", "Clear", WeatherIcon::ClearNight},
      {"partlycloudy", "Partly", WeatherIcon::PartlyCloudy},
      {"cloudy", "Cloudy", WeatherIcon::Cloudy},
      {"rainy", "Rain", WeatherIcon::Rainy},
      {"pouring", "Pouring", WeatherIcon::Pouring},
      {"snowy", "Snow", WeatherIcon::Snowy},
      {"snowy-rainy", "Snow", WeatherIcon::Snowy},
      {"fog", "Fog", WeatherIcon::Fog},
      {"windy", "Wind", WeatherIcon::Windy},
      {"windy-variant", "Wind", WeatherIcon::Windy},
      {"lightning", "Storm", WeatherIcon::Lightning},
      {"lightning-rainy", "Storm", WeatherIcon::Lightning},
  };
  v.icon = WeatherIcon::Unknown;
  v.cond = humanise(s);
  for (const Row &r : rows) {
    if (s == r.state) {
      v.cond = r.text;
      v.icon = r.icon;
      break;
    }
  }
  if (st->humidity) v.hum = pct(int(std::lround(*st->humidity)));
  return v;
}

VacuumView vacuumView(const EntityState *st) {
  VacuumView v;
  v.look = lookOf(st);
  if (v.look == Look::Loading) return v;
  if (v.look == Look::Unavailable) {
    v.status = "n/a";
    return v;
  }
  const std::string &s = st->state;
  if (s == "unknown") {
    v.status = "-";
    return v;
  }
  if (s == "cleaning") { v.status = "Cleaning"; v.startFilled = true; }
  else if (s == "docked") { v.status = "Docked"; v.dockFilled = true; }
  else if (s == "idle") v.status = "Idle";
  else if (s == "paused") { v.status = "Paused"; v.pauseFilled = true; }
  else if (s == "returning") { v.status = "Returning"; v.dockFilled = true; }
  else if (s == "error") {
    v.status = st->status.empty() ? "Error" : st->status;
    v.error = true;
  } else v.status = humanise(s);
  if (!v.error && st->battery_level) {
    v.showBattery = true;
    v.battery = *st->battery_level;
    v.lowBattery = v.battery <= 20 && s != "docked";
  }
  return v;
}

}  // namespace uistate
