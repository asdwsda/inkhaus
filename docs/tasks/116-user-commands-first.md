# 116: User commands jump the net queue

Status: todo
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
