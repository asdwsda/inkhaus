# 051: Streaming HTTP for history fetches

Status: todo
Depends on: 046
UI spec: none
Needs device: yes (final check)

## Scope
Per 046 task split item 4: a streaming request in ha_client/HttpTransport with a sink callback, per-request byte limit (512 KB) and deadline (15 s), cancel flag; Arduino implementation without the 64 KB cap for that mode only; a net task job type "history fetch". Normal calls keep the 64 KB cap.

Research and design: 046-history-research.md (accepted by the user 2026-10-29).

## Files to touch
- lib/ha_client, src/ha_transport_arduino.*, lib/app_logic (job), src/net_task.*, tests

## Acceptance criteria
- [ ] Native tests with a fake transport: chunks reach the sink, byte limit and cancel abort cleanly, normal calls unchanged.
- [ ] Device (later): `HIST: fetch_ms bytes` logged for a 24 h fetch over LAN and remote.

## Notes
