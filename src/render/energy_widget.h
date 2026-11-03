#pragma once
// Energy widget (2x1, task 109, docs/ui/widgets-new.md section 8). Own file so the other new widgets merge cleanly.
#include "app_state.h"
#include "draw.h"
#include "energy_view.h"
#include "ui_layout.h"

namespace render {

// Outline frame, two bar rows (label, track, INK fill, value); dotted frame + `n/a` + warn badge when unavailable.
void drawEnergyWidget(M5Canvas &c, const ui::Rect &r, const uistate::EnergyView &v);

}  // namespace render
