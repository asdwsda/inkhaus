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
