#pragma once
// View model and layout of the weather widget with forecast (task 104, docs/ui/widgets-new.md section 2).
// Pure text / number logic, no drawing. The left block is the 1x1 weather block (uistate::weatherView).
#include <string>
#include <vector>

#include "forecast.h"
#include "entity_state.h"
#include "widget_view.h"

namespace forecast {

struct ColumnView {
  std::string day = "-";   // "Tue", "-" when neither forecast nor clock is known
  bool hasIcon = false;    // false: forecast missing or condition unknown
  uistate::WeatherIcon icon = uistate::WeatherIcon::Unknown;
  std::string hilo = "-";  // "14°/6°", "14°" (no low), "-"
};

struct ForecastView {
  uistate::WeatherView now;  // look, temp, unit, cond, icon of the current block
  bool forecastMissing = false;  // call failed / not loaded: day names kept, icons dropped, hi/lo "-"
  ColumnView cols[kDays];
};

// st: the weather entity (nullptr = no cached value). data: its forecast (nullptr = none). today: local date
// (invalid = clock unset). Unavailable entity: no forecast columns are drawn (look says so). Loading: "-" columns.
ForecastView forecastView(const EntityState *st, const Data *data, const Ymd &today);

struct ColumnRect {
  int cx = 0;        // centre x (widget-relative)
  int dayCy = 24;    // F18 centre line
  int iconX = 0, iconY = 40;  // S icon (24)
  int hiloCy = 80;   // F18 centre line
};

struct Layout {
  int cw = 0, fx = 0;           // column width, forecast start x
  int iconX = 12, iconY = 28;   // L icon 48
  int tempX = 68, tempBaseline = 53;  // F40 value + F24 unit
  int condX = 68, condBaseline = 84;  // F18, max width condMaxW
  int condMaxW = 0;
  int divX = 0, divY = 20, divW = 1, divH = 64;
  int unavailTempRight = 0, unavailBaseline = 66;  // `n/a` F40 right edge (W - 13)
  int badgeX = 68, badgeY = 57;
  ColumnRect cols[kDays];
};

// W = widget width in px (508 portrait, 552 landscape; wider widgets keep the columns at the right edge).
Layout layoutFor(int W);

}  // namespace forecast
