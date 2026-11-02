# 086: Keep the current tab across sleep

Status: todo
Depends on: 071 (graphs), 084 (power-off wake)
UI spec: none
Needs device: yes

## Scope
User 2026-11-02: going to sleep must not switch back to the first tab. The device sleeps and wakes on the tab
(and sub-page / scroll position, if any) that was shown. Find where it resets today (sleep path, wake/boot
restore, or the idle timeout) and remove that.
- Persist the shown tab across every sleep kind: deep sleep (RTC memory) and power-off (NVS, RTC memory is lost);
  write only when it changed. A config change that removes the tab falls back to the first tab.
- Scheduled wakes and interactive wakes update the graphs of the shown tab only (kGraphFetchAllTabs = false,
  graph_widgets.h). With the device left on a tab with graphs, its graphs update on every scheduled wake.
  A tab without graphs costs no history fetch.
- Popups/Settings: if sleep happens while one is open, wake on the tab under it (Settings is left on sleep).
- Out of scope: changing the graph fetch policy.

## Files to touch
- src/main.cpp, src/sleep.cpp (or session_glue), lib/ui_state/src/app_state.* (restore), lib/app_logic/ if logic
- test/

## Acceptance criteria
- [ ] Native tests: restore picks the saved tab; invalid/removed tab -> first tab; graph fetch list = shown tab only.
- [ ] Device: on tab 2 (e.g. Stats), sleep by idle timeout -> wakes on tab 2, both from deep sleep and power-off.
- [ ] Device: left on a tab with a graph over a scheduled wake -> the wake log shows hist_n >= 1 and the graph
      redraws; left on a tab without graphs -> hist_n = 0.
- [ ] `pio run` and `pio test -e native` pass.

## Notes
