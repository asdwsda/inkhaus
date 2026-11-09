# 091: Scheduled wake switches back to the first tab

Status: shipped v1.4.0, device check pending
Depends on: 086, 083
UI spec: none
Needs device: yes (fix is PC-first; device check later)

## Scope
User 2026-11-02, during the 083 look test (env `m5paper-epdoff` from a8e6bb9, settings unchanged): device asleep on
Stats (tab 1); the 17:00 scheduled wake (first one after the 16:10 start) redrew the screen on Home (tab 0).
086 verified the tab across rocker wakes only. Suspects: scheduled-wake path skips the tab restore; the
083 periodic GC16 full (unknown last-full = due) or prepareSleep(forceFull) renders the default tab; the
epdoff hardware-reset/cold bootFrame path doesn't count as a "sleep wake" for `slp/tab`.
- Find the cause, fix it for every build (normal and `EPD_SLEEP_OFF`) and every wake kind.
- Out of scope: the look test itself.

## Files to touch
- src/main.cpp, src/boot.cpp, src/sleep.cpp, src/present.cpp (as needed), lib/app_logic/ if logic
- test/

## Acceptance criteria
- [x] Notes name the root cause (file:line).
- [x] Native test covers the restore decision for timer wake (and the periodic-full wake) -> saved tab.
- [x] Device (debug build = normal code + serial cmds; epdoff via m5paper-debug + EPD_SLEEP_OFF/NO_PERIODIC_FULL flags): asleep on Stats, a scheduled wake (short test interval ok) keeps Stats; `BOOT: tab_restored=1` in the log;
      checked on the normal build and the epdoff build.
- [ ] `pio run` (all envs) and `pio test -e native` pass.

## Notes
- 2026-11-02 (firmware-dev, PC only, no device/serial): no timer-wake-specific bug found by reading. Timer wake (deep sleep, `pin=timer`) and
  power-off alarm wake both classify as wokeFromSleep (lib/app_logic/src/sleep_logic.cpp:26) so the restore at src/main.cpp:1059 runs.
  ROOT CAUSE (most likely, UNVERIFIED): the restore was gated on `wokeFromSleep()` (sleep_logic.cpp:26-29), which is false for any restart
  inside a session: ResetKind::Software/Crash/Other (brownout, ESP_RST_BROWNOUT -> Other, sleep.cpp:41-53) classify as ColdBoot
  (sleep_logic.cpp:22) and splashWanted is false for them, so the screen is redrawn on Home without a splash (matches the report), and the
  following sleep's `saveTab(shownTab()=0)` (main.cpp:928) overwrites the saved tab. A scheduled wake is the risky one (Wi-Fi start on
  battery = brownouts, 079). Not confirmable without the wake log (`WAKE: reset=` / `BOOT: tab_restored` of the 17:00 wake).
  Fix: `app_logic::restoreTabWanted` (sleep wakes + Software/Crash/Other restarts; not reset button / cold boot), used at main.cpp:1059;
  `saveTab` also in showFrame (main.cpp ~300) so a mid-session restart finds the current tab. Test: test_restore_tab_decision.
  Device check: if the 17:00 log shows reset=deepsleep pin=timer and still Home, the cause is elsewhere (reopen).
- 2026-11-02 (device part): wake log of the 083 look test (epdoff build): the 17:00 wake row is `cold_boot, interactive, reset_reason=TASK_WDT`
  (epoch 17:00:40, preceded by the 16:30 rtc_alarm POWERON wake that kept working). So it was a task-watchdog restart inside setup (15 s WDT),
  not a normal timer wake: matches the hypothesis (restart classified ColdBoot -> Home, no tab restore). Cause of the WDT itself is not
  investigated here (crash counter / 079 area). Fix verified: with Keep awake Off, Power off mode, interval temporarily 2 min (local
  hack in settings.h, reverted): asleep on Stats -> scheduled wake `reset=deepsleep pin=timer`, `BOOT: tab_restored=1`, `render tab=1`, one history
  fetch (graph_batch n=1), re-slept; same on the EPD_SLEEP_OFF+NO_PERIODIC_FULL build. Not tested: plain m5paper binary (needs a touch on Stats;
  code identical apart from DEBUG_SERIAL), mid-session `reboot` restart on device. Logs: scratchpad wake_debug.log, wake_epdoff.log, wakelog.csv.
  User settings restored (Power off, 30 min, Keep awake On USB); final flash = m5paper.
- 2026-11-02 (architect): root cause CONFIRMED by the wake log: 17:00 row reset_reason=TASK_WDT (epdoff build, Power off mode,
  16:30 rtc_alarm wake was fine). Why the watchdog fired is a separate bug: task 093.
