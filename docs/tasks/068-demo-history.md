# 068: Demo mode shows history charts

Status: todo
Depends on: 034 (demo mode), history widgets (docs/ui/history.md)
UI spec: docs/ui/history.md (no new screens)
Needs device: yes (visual check)

## Scope
User (2026-10-30): in demo mode the history charts have no data. Generate plausible synthetic history for every demo
entity that has a history/chart widget (line, step, bar per docs/ui/history.md): e.g. temperature daily curve with noise,
humidity, power with on/off steps, binary sensors as steps. Deterministic (seeded per entity) so screenshots are stable,
anchored to the current time so the charts look live; no network in demo mode.
Out of scope: changes to the real history fetch path.

## Files to touch
- lib/ (pure generator + native tests), the demo data source in src/, demo.yaml if widgets need adding

## Acceptance criteria
- [ ] Native tests: generator output deterministic, within each sensor's range, correct point count/time span per chart type.
- [ ] Demo mode on the device: every chart widget in the demo tabs shows data in both orientations (user check).
