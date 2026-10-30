# 074: Graph widget redraw in one refresh

Status: in-progress
Depends on: 071
UI spec: docs/ui/history-widget.md (Redraw: one epd_quality partial refresh of the widget)
Needs device: yes

## Scope
When a graph widget's data changes, refresh the e-ink panel once for the whole widget (or the plot
rect) with `epd_quality`, instead of the 3-4 small diff rects of about 440 ms each that 071 does now
(1.3-1.7 s per wake, measured 2026-10-30). Other widgets keep the normal frame diff.
Out of scope: changing graph fetch rules, refresh modes of other widgets.

## Files to touch
- src/ render/screen code (frame diff / refresh rect merge), lib/ helpers if the merge logic is pure.

## Acceptance criteria
- [ ] Native test: dirty rects inside one graph widget are merged into a single refresh rect; rects of
  other widgets are unaffected.
- [ ] Device: on a wake with changed graph data, the debug log shows one `epd_quality` refresh for the
  graph widget; the extra redraw time drops from about 1.3-1.7 s to at most about 0.5 s.
- [ ] Device: no ghosting or missing pixels in the graph after the merged refresh (user visual check).
- [ ] `pio test -e native` and `pio run` pass.

## Notes
- Measurement source: 071 Notes (device measurement, 2026-10-30).
