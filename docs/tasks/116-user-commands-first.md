# 116: User commands jump the net queue

Status: PC part done (device check open, docs/device-checks.md 116)
Depends on: 115
UI spec: none
Needs device: yes (timing check on battery; PC part first)

## Scope
Task 115 found one FIFO net worker (src/net_task.cpp): a tap's service call waits behind the post-wake state fetch,
graphs, report (100), notifications (103), forecast (104), calendar/events and the OTA check, and behind their 4 s connect /
8 s read timeouts on a weak link (RSSI -82 dBm). Decision (architect, 2026-11-03):
- Two lanes: user commands (service calls from taps/popups, refresh-now) always run before background jobs.
  The worker picks the user lane first at every job boundary.
- Background jobs that read long bodies (history/statistics, events, notifications, OTA) check a cooperative
  "user waiting" flag between reads/chunks and yield: the job is aborted and re-queued (or dropped for the session if it
  has no value later), never blocking a tap for more than one chunk read.
- The OTA check never runs in an interactive session: only on scheduled wakes (or Settings > Check now).
- The post-wake state fetch stays first (the tap needs a connection anyway) but a tap queued during it runs next.
- `timing` lines get `queue_wait_ms` (enqueue -> start) per command.
- Out of scope: Wi-Fi connect time itself (2.2 s on button wakes vs 0.8 s on alarm wakes; separate follow-up if needed).

## Acceptance criteria
- [ ] Pure queue/priority logic in lib/ with native tests: a tap enqueued behind 3 background jobs starts next;
      a yielding job is re-queued or dropped per its policy; OTA job refused in interactive sessions.
- [ ] `pio test -e native`, `pio run` all envs.
- [ ] Device: on battery, rocker wake, 10 taps: tap -> call start <= 300 ms and tap -> action <= 2 s in 9/10 (`timing`).

## Notes
- User 2026-11-03: device and router stay where they are, so the weak link (RSSI about -82 dBm) is the normal case; design and test for it.
Review fixes (architect decisions, 2026-11-03):
- g_yielded is cleared per job in runJob() (next to g_timedOut), so Check now after a yielded auto check is not misread.
  Not unit-testable (hardware file); the retry rule stays in tick().
- The automatic OTA check no longer blocks loop(): tick() only plans it (poster -> NetTask::postOtaCheck, background lane,
  NetCommand::Type::OtaCheck, not tied to the session epoch). The net task closes the HA socket, then calls
  ota::runAutoCheckJob() (own job task as before; yields via userWaiting()). Result hand-off is a volatile state
  (Queued/Done) polled by tick(), not a NetEvent (no driver changes); busy() holds sleep from post until tick() sees Done.
  60 s idle + empty user lane guard kept. Check now stays a blocking user action in loop (never yields).
  Install still blocks loop() (modal progress screen, by design): reported, not changed.
- A History/Forecast job dropped after 3 yields arrives as Cancelled: popup/graph keep their data (no offline flag; popup
  without data re-plans the fetch), forecast is unmarked as asked and retried later. Tests added (popup, graphs, forecast, lanes).
