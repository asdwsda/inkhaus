# 043: Per-tab landscape columns (4 or 5)

Status: in-progress
Depends on: 011, 009
UI spec: docs/ui/layout.md (4-column landscape grid, from docs/ui/proposal-landscape-4col.svg)
Needs device: yes (later)

## Scope (ARCHITECTURE.md 2026-10-29)
- lib/config: optional tab key `landscape_columns` (4 or 5, default 5; validated with path + line; error
  messages without values); config cache format bump; tests.
- lib/ui: the grid becomes a function of (orientation, columns). Landscape 4 columns: cells 223×104,
  x = 16, 251, 486, 721, rows as today; wide widgets (cover, climate, vacuum) 2 cells; sub-regions for the
  narrower wide widgets (cover buttons at W−228…, climate cluster per the proposal; flame on the name line
  when line 2 is full). The size table per (orientation, columns). Tests incl. section 8 style vectors for
  the 4-column grid (from the proposal numbers) and the property test over both grids.
- src/render: climate flame placement for the narrow variant; nothing else should need changes.
- Spec: ui-designer moves the proposal numbers into layout.md section 2/3/8 (parallel).

## Acceptance criteria
- [ ] Native tests for config + layout; `pio test -e native` and the builds pass.
- [ ] Screenshot (m5paper-debug, later) of a 4-column tab matches docs/ui/png/proposal-landscape-4col.png.

## Notes
