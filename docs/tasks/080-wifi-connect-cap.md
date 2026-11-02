# 080: Cap the Wi-Fi connect time per session

Status: todo
Depends on: 047, 056
UI spec: none (uses the existing offline/error indicator)
Needs device: yes (short check)

## Scope
`net_policy.h` has an 8 s budget per attempt, but the wake log shows wifi_ms up to 80 s in a session (p90 8.6 s,
median 4 s) and 4/117 failed fetches: retries keep the radio on. Cap the total time spent connecting per wake.

- Total connect budget per wake (default 15 s, a constant in lib/net). After it, stop retrying, show the existing
  offline indicator, and in a scheduled wake go back to sleep at once.
- Interactive sessions: the user must still be able to send commands any time while awake. A tap that needs HA
  while offline starts one fresh connect attempt (same budget) instead of being dropped.
- Out of scope: idle timeout (user: keep as is), access-point selection logic.

## Files to touch
- lib/net/src/net_policy.{h,cpp} (budget logic, natively tested)
- src/net.cpp, src/net_task.cpp (apply it)
- test/ (native tests)

## Acceptance criteria
- [ ] Native tests: retries stop after the session budget; a user action after giving up starts a new attempt;
      the per-attempt limits (8 s total, 3 s cached) still hold.
- [ ] A scheduled wake with no Wi-Fi sleeps again within budget + ~3 s (wake log awake_ms).
- [ ] Interactive wake with the AP off: offline indicator after ~15 s; turning the AP on and tapping a widget
      connects and sends the command.
- [ ] `pio run` and `pio test -e native` pass.

## Notes
- First find out where the 80 s came from (retry loop, scan fallback, or the TLS/WS step counted in wifi_ms).
