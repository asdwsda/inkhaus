# Refactor survey (task 124 step 1) - read-only, 2026-11-05
Coverage legend: native = covered by `pio test -e native`; none = device-only (src/ glue/Arduino code has no native test).

## Ranked
1. Split main.cpp loop()/setup() (src/main.cpp:1393 loop 230 lines; :1188 setup 195 lines; file 1622 lines, 505 g_ refs, 58 *_glue:: calls, 21 #if/Serial sites).
   Benefit: high (largest file, 2 of the 3 largest functions in repo). Risk: M-H (ordering of pumps/sleep/present is subtle). Effort: L. Tests: none (device only); only the extracted pure decisions (app_logic, power) are native-tested.
   Cut: extract loop() into phases (input, net events, pumps, frame, sleep), setup() into boot/config/restore/net-start; move goToSleep (:1083, 67), settings glue (:953-1050), graphs*/history* pumps (:420-650) to own files.
2. Duplicate confirm dialog: render/notif_widget_draw.cpp:73 drawConfirm == render/secure_widget.cpp:150 drawConfirmDialog (identical but Btn::Outline vs false);
   layout notifConfirmLayout (lib/ui_state/src/notif_popup.cpp:47) vs uistate::confirmLayout (confirm_dialog.h); kNotifConfirmTimeoutMs (notif_popup.h:16) == kConfirmTimeoutMs (confirm_dialog.h:16); same "Skips by itself after 15 s", same Yes/No/Skip keys.
   Benefit: M (one dialog, one place for STYLE 10b). Risk: L (pixel-identical layout numbers differ? check: notif panel {200,188,560,220}). Effort: S-M. Tests: native for layout/state (test_notifications_widget, test_ui_layout, test_secure_widgets); drawing none.
3. Per-widget glue triplet src/calendar_glue.cpp (96), forecast_glue.cpp (89), art_glue.cpp (96): same shape (g_store, pump -> postX, onResult -> store.set + logf, fillDemo, newSession); calendar vs forecast diff only in types/field names (+ forecast has Cancelled/unmarkAsked handling at :51, calendar lacks it: possible latent inconsistency, ask).
   Benefit: M. Risk: M (template over store/command types; each has quirks). Effort: M. Tests: store logic native (test_calendar, test_forecast, test_media_art); glue none.
4. Release-build logging: Serial.printf is unconditional in release (94 Serial.print sites; Serial.begin always, boot.cpp:419). logMem("tick") every 30 s while awake (main.cpp:1401-1403) plus 7 logMem calls (:732,1277-1320) and loop_stack_free (:1398) run in release; only shot.cpp and ota.cpp:40 are #ifdef DEBUG_SERIAL. Costs: heap_caps calls + UART blocking each 30 s, flash size, awake-time power.
   Fix: a LOG macro/wakelog level, logMem/tick under DEBUG_SERIAL (or the debug_log setting). Benefit: M (power/size). Risk: L (diagnostics lost in release; user may want MEM in field logs). Effort: S. Tests: none; compare binary size.
5. Other long functions: lib/config/src/config_cache.cpp:171 deserializeConfig 136; src/ota.cpp:474 installBody 128; lib/ha_client/src/ws_frame.cpp:32 FrameDecoder::feed 122 (state machine, leave); src/boot.cpp:409 begin 118; lib/ui/src/ui_rocker.cpp:122 focusHandle 111; src/render/graph_widget.cpp:91 drawGraphWidget 110; src/present.cpp:487 bootFrame 106; lib/settings_ui/src/settings_ui.cpp:80 computePageLayout 106 (data table-like, low value).
   Benefit: L-M each. Risk: M-H for boot/ota/present (device-only), L for deserializeConfig/focusHandle. Effort: M each. Tests: native for config_cache (test_config_cache), ui_rocker (test_ui_layout/test_ui_state), ws_frame (test_ws_client); boot/ota/present/graph draw none (ota_logic native for the pure part).
6. Large lib files: lib/config/src/config.cpp 956 (parser; split YAML parse vs validation), lib/settings_ui/src/settings_ui.cpp 862, src/present.cpp 811, lib/ui_state/src/app_state.cpp 682, src/ota.cpp 625. Benefit: M (navigation). Risk: L for pure lib files. Effort: M. Tests: native for config, settings_ui, app_state; present/ota partly.
7. Rendering draw helpers: render/widgets.cpp:181,286 and 244/346 repeat per-orientation icon/name/state blocks and drawBadge calls with many "UNVERIFIED position" magic numbers; render/ code uses ad hoc `struct B {...}` button rows (also in 2). Benefit: L-M. Risk: M (pixel changes). Effort: M. Tests: layout native; drawing none.
8. Big test files: test_ui_layout 2194, test_app_logic 1566, test_ui_state 1412, test_ha_client 1219 lines. Splitting by feature is low risk but low value; keep.

## Dead code
No unreferenced functions found in src/ (name-count scan; only SDK calls appear once). No #if 0 / TODO / FIXME. 12 "UNVERIFIED" comments (net.cpp:274,317; input.cpp:20; sleep.h:40; battery_limit.cpp:13; sht30_i2c.*; boot.cpp:337,340; widgets.cpp:181,244,286,346,457; session_glue.cpp:91): stale-check candidates, not removable code. Build-flag debug paths (OTA_TEST_BAD at main.cpp:1406, shot.cpp) are intentional. lib/ not scanned for unused symbols beyond this.

## Sizes
src/*.cpp+render total 11384 lines; tests ~34k lines (53.6k total). Largest: main.cpp 1622, present.cpp 811, ota.cpp 625, boot.cpp 535, render/widgets.cpp 496, net.cpp 467.
