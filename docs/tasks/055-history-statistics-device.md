# 055: Statistics source on the device (history phase 2 wiring)

Status: todo
Depends on: 053, 054
UI spec: docs/ui/history.md
Needs device: yes

## Scope
- `WsSocket` over WiFiClientSecure (CA bundle, internal/external URL as the session uses; ws:// for http).
  Close the HTTP keep-alive socket before the WS connect if the free internal heap is below the margin.
- Net-task job for a statistics fetch (same ticket/cancel/limits as 053), source/period choice from 054,
  popup: drop `needsStatistics`, bars enabled, "no statistics for this sensor" message, cache as 053.
- Log `HIST: src=stats fetch_ms parse_ms bytes points heap_min err` (no URL/token).

## Files to touch
- src/ (transport, net_task, main), lib/ui_state/src/history_popup.*, tests

## Acceptance criteria
- [ ] Native tests for the popup source choice and the new outcome.
- [ ] Device: 7 d line and a daily bar graph load; heap_min stays above the safety margin; a normal
      tap action right after still works.

## Notes
- From the 054 review (fold in here): use esp_random for the handshake key and the frame masks
  (ws_client.cpp:225); clamp huge numbers before int64/float casts (stats_parser.cpp:130,169,185);
  scrub() the server error code (ws_client.cpp:376). Socket: read returns 0 on timeout, <0 on close;
  Request.host = HA host[:port]; align the window start to the period boundary for bars.
- 054 line columns: min/max from the stat min/max, last = mean (accepted).
- 2026-10-30 tester review of 1405b6d: no blockers. To fix with the device check:
  F1 always close the HTTP keep-alive socket before the WS TLS connect (the 70 KB free-total check
  ignores fragmentation; architect decision: simpler and safe, a history popup is rare). F2 bars are
  aligned to UTC days; HA day buckets start at local midnight: use the local offset in barWindow.
  F3 heap_min is the since-boot minimum. No native coverage for the Arduino socket/connector.
