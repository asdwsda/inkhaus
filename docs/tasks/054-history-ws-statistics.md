# 054: History from WebSocket statistics (phase 2)

Status: todo
Depends on: 050, 051
UI spec: docs/ui/history.md
Needs device: yes

## Scope
Per 046 task split item 7: short-lived WebSocket client (auth, one recorder/statistics_during_period command, close), parsing, `source: statistics` and auto for >24 h, bars, 7 d / 30 d. Heap check for the second TLS session.

Research and design: 046-history-research.md (accepted by the user 2026-10-29).

## Files to touch
- lib/ha_client (ws), src/ (transport), lib/history (statistics parse), tests

## Acceptance criteria
- [ ] Native tests for the WS message flow and statistics parsing.
- [ ] Device: 7 d graph loads; heap stays above the safety margin.

## Notes
