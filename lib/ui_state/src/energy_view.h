#pragma once
// Energy widget (task 109, docs/ui/widgets-new.md section 8): view model and layout. Pure logic, no drawing.
// Two bar rows (usage, solar) with one shared scale. The values are "today so far" from statistics (change since
// local midnight, so a total_increasing meter works); see graph_widgets (entries keyed kEnergySolarKey).
#include <string>

#include "entity_state.h"
#include "graph_widgets.h"
#include "ui_layout.h"
#include "widget_view.h"

namespace uistate {

enum class EnergyRowState { Hidden, Loading, Value, NoValue };  // Hidden: not configured

struct EnergyRowView {
  EnergyRowState state = EnergyRowState::Hidden;
  double kwh = 0;
  std::string text = "-";  // "7.4 kWh", "11.2 kWh", "112 kWh", "-"
  int fillW = 0;           // px of the INK fill inside the track (0 = empty track)
};

struct EnergyView {
  Look look = Look::Loading;  // Unavailable: every configured sensor is unavailable (dotted frame, `n/a`)
  EnergyRowView row[2];       // 0 usage, 1 solar
  int rows = 0;               // configured rows (1 or 2)
};

struct EnergyInput {
  bool configured = false;
  const EntityState *state = nullptr;  // nullptr = nothing cached yet
  bool hasValue = false;
  double value = 0;  // today so far, in the unit of the entity
};

// Wh -> kWh (/1000), MWh -> kWh (*1000), anything else is taken as kWh.
double toKwh(double v, const std::string &unit);
// "7.4 kWh"; |value| >= 100: no decimal.
std::string energyText(double kwh);
// Today's value of an energy graph entry: the last bucket of the one-day series. False without data.
bool energyTodayValue(const GraphEntry *e, double &out);
// bw = track width in px (energyLayout(W, rows).bw).
EnergyView energyView(const EnergyInput &usage, const EnergyInput &solar, int bw);

struct EnergyRowRect {
  int cy = 0;          // centre line
  ui::Rect track;      // widget-relative
  int labelX = 12;
  int valueRight = 0;  // right edge of the value text
};

struct EnergyLayout {
  int W = 0;
  int bw = 0;  // track width = W - 188
  EnergyRowRect row[2];
  int naCy = 52, naRight = 0;      // `n/a` F24 centre line / right edge
  int badgeX = 0, badgeY = 12;     // warn badge (24 px icon)
};
// rows = 2: centres 32 / 72. rows = 1: the one configured row is drawn centred (cy 52) in row[0] / row[1] as configured.
EnergyLayout energyLayout(int W, int rows);

}  // namespace uistate
