# 104: Weather widget with forecast

Status: PC part done (device check pending)
Depends on: 045
UI spec: docs/ui/widgets-new.md §0, §2 (+ mockups docs/ui/png/widgets-new-*.png)
Needs device: yes (look + a real entity; PC part first)

## Scope
Wide widget, current + 3-day forecast. Forecasts are no longer weather attributes in current HA: fetch them with `weather.get_forecasts` (type daily) via REST `POST /api/services/weather/get_forecasts?return_response`, only when the widget is on the shown tab and at most once per scheduled wake. Missing small weather icons (snowy, fog, windy, lightning, pouring at S size) drawn per STYLE.md.
Follow the shared rules (§0) and grid sizes (§9) in docs/ui/widgets-new.md, existing widget/popup code patterns.
Out of scope: other new widgets.

## Files to touch
- lib/config (widget type + options), lib/ (widget logic/layout), src/render (drawing), test/

## Acceptance criteria
- [ ] Config parsing + widget logic (state mapping, texts, fallbacks: unavailable/loading) covered by native tests.
- [ ] Layout rects match the spec numbers (tests where the layout lives in lib/).
- [ ] `pio test -e native`, `pio run` (all envs) pass.
- [ ] docs/CONFIG.md + config.example.yaml document the widget; docs/device-checks.md line; UNVERIFIED tags.

## Notes
- PC part done. New: lib/forecast (request, parser, day selection, Store, fetch rule), lib/ui_state/src/forecast_view.* (view + layout), src/forecast_glue.* (pump/handler), src/render/weather_forecast_draw.*. Shared-file edits: Widget.forecast (config model/parse/cache bit 7 of the display byte, no format bump), `ui::forecastSizeLimits` + 1 line in resolveSize, HaClient::call_service_response, NetCommand/NetEvent Forecast, HaWorker::runForecast, SessionDriver handler, AppState forecast store + WidgetData.forecast/today, 1 case in drawWidget, 6 lines in main.cpp.
- Spec says WS call_service; the task says REST, REST used. Forecast is RAM only (not in the RTC/SD state cache). Pouring/windy glyphs hand-drawn, not in icon_data (no regeneration, avoids merge conflicts).
