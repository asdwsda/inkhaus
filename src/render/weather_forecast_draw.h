#pragma once
// Weather widget with forecast, wide (task 104, docs/ui/widgets-new.md section 2). Layout and texts come from
// lib/forecast (forecast_view); this file only draws.
#include "app_state.h"
#include "draw.h"

namespace render {

// True when the weather widget `w` of width `widthPx` is drawn with the forecast layout.
bool isForecastWidget(const config::Widget *w, int widthPx);
// Draws frame, current block and the 3 forecast columns into the widget rect `r` (absolute).
void drawWeatherForecast(M5Canvas &c, const ui::Rect &r, const uistate::WidgetData &d);

}  // namespace render
