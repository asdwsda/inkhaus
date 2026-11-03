#include "energy_view.h"

#include <algorithm>
#include <cmath>

namespace uistate {

double toKwh(double v, const std::string &unit) {
  if (unit == "Wh") return v / 1000.0;
  if (unit == "MWh") return v * 1000.0;
  return v;
}

std::string energyText(double kwh) {
  const int dec = std::fabs(kwh) >= 100.0 ? 0 : 1;
  return fmtNumber(kwh, dec) + " kWh";
}

bool energyTodayValue(const GraphEntry *e, double &out) {
  if (!e || !e->hasData || e->series.buckets.empty()) return false;
  const history::Bucket &b = e->series.buckets.back();
  if (!b.has) return false;
  out = b.last;
  return true;
}

namespace {
EnergyRowView rowOf(const EnergyInput &in) {
  EnergyRowView r;
  if (!in.configured) return r;  // Hidden
  if (in.state && !in.state->available) {
    r.state = EnergyRowState::NoValue;  // this sensor is unavailable: its own row shows "-"
    return r;
  }
  if (in.hasValue) {
    r.state = EnergyRowState::Value;
    r.kwh = toKwh(in.value, in.state ? in.state->unit_of_measurement : std::string());
    r.text = energyText(r.kwh);
  } else {
    r.state = in.state ? EnergyRowState::NoValue : EnergyRowState::Loading;
  }
  return r;
}
}  // namespace

EnergyView energyView(const EnergyInput &usage, const EnergyInput &solar, int bw) {
  EnergyView v;
  v.row[0] = rowOf(usage);
  v.row[1] = rowOf(solar);
  v.rows = (usage.configured ? 1 : 0) + (solar.configured ? 1 : 0);
  bool allUna = v.rows > 0, allLoading = v.rows > 0;
  double mx = 0;
  const EnergyInput *in[2] = {&usage, &solar};
  for (int i = 0; i < 2; ++i) {
    if (!in[i]->configured) continue;
    const bool una = in[i]->state && !in[i]->state->available;
    allUna = allUna && una;
    allLoading = allLoading && v.row[i].state == EnergyRowState::Loading;
    if (v.row[i].state == EnergyRowState::Value) mx = std::max(mx, v.row[i].kwh);
  }
  v.look = allUna ? Look::Unavailable : allLoading ? Look::Loading : Look::Normal;
  if (mx > 0)
    for (EnergyRowView &r : v.row)
      if (r.state == EnergyRowState::Value && r.kwh > 0) {
        const int w = int(std::lround(double(bw) * r.kwh / mx));
        r.fillW = std::min(bw, std::max(2, w));
      }
  return v;
}

EnergyLayout energyLayout(int W, int rows) {
  EnergyLayout l;
  l.W = W;
  l.bw = W - 188;
  for (int i = 0; i < 2; ++i) {
    EnergyRowRect &r = l.row[i];
    r.cy = rows == 1 ? 52 : (i == 0 ? 32 : 72);
    r.track = ui::Rect{76, r.cy - 10, l.bw, 20};
    r.valueRight = W - 12;
  }
  l.naRight = W - 12;
  l.badgeX = W - 36;
  return l;
}

}  // namespace uistate
