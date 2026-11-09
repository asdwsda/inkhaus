# 082: History graph cost in the wake log

Status: shipped v1.3.0, night run pending
Depends on: 029 (wake log), 071 (history graph)
UI spec: none
Needs device: yes (A/B over two nights)

## Scope
Measure what the history graph widget costs in battery. Sleep current is the same either way, so the cost is the
extra awake time (history fetch + parse + graph draw) per wake.

- Add wake-log columns: `hist_n` (history fetches this wake), `hist_ms` (sum of their fetch+parse time),
  `hist_bytes` (sum of response bytes). Append at the end of the CSV row (keep old columns and order;
  tools/wakelog.py and any parser must still read old rows).
- Out of scope: changing the history fetch policy.

## Files to touch
- lib/app_logic/src/wakelog_csv.{h,cpp}
- src/wakelog.{h,cpp}, src/net_task.cpp (feed the numbers; net_task already logs fetchMs/parseMs)
- test/ (native test for the CSV row)

## Acceptance criteria
- [ ] Native test: header and row include the 3 new columns at the end; defaults when no history fetch (0,0,0).
- [ ] `pio run` and `pio test -e native` pass.
- [ ] Device: a scheduled wake on a tab with a graph logs hist_n >= 1 and plausible hist_ms/hist_bytes.
- [ ] A/B (user + architect): night 1 with the graph tab, night 2 without, same refresh interval; compare median
      awake_ms of scheduled wakes and mV/h. Result recorded in Notes.

## Notes
- User 2026-11-02: baseline = 10-30..11-02 run (graph on Home; scheduled wakes median awake 6674 ms, p90 8435,
  9.8 mV/h over 67 h). Only the "no graph on Home" night is still needed; compare scheduled-wake awake_ms.
