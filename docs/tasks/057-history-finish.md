# 057: History finishing touches

Status: todo
Depends on: 049, 052, 053, 055
UI spec: docs/ui/history.md
Needs device: yes (short visual check)

## Scope
1. **Unavailable spans** (spec line 95): `unavailable`/`unknown` periods draw no line/band and a 4 px MID
   bar at plot bottom - 8 over their width. Needs a per-column "unavailable" flag in history::Series
   (collector + cache format; bump the cache version so old files read as Missing). Step charts too.
2. **Config checks (049 open item):** `statistic`, `period` and `source` combinations that make no sense
   (e.g. `source: history` with `type: bar`, `statistic` on a step chart, `period` on a line) are config
   errors with the usual path message, or are documented as ignored; pick the stricter one where the user
   clearly made a mistake (bar + history source), ignore harmless ones. README config table updated.
3. **Bar range line 2** (spec line 55): `total 47.3 kWh` for `sum`/`change`; `min x max y` of the bars for
   mean/min/max. Default statistic `sum` for an energy device_class if the device class is known to the
   firmware (check the state fetch/template; add it only if cheap), else mean.
4. **Hourly bars over 128 h:** clamp at config parse (hours <= 128 with `period: hour`), or document.

## Files to touch
- lib/history, lib/config, src/render/popup_history.cpp, README.md, tests

## Acceptance criteria
- [ ] Native tests: unavailable flag through collector, cache round-trip (and old-version rejection),
      config error cases, bar range line text.
- [ ] `pio test -e native`, `pio run` pass.
- [ ] Device: a sensor with an unavailable period shows the MID marker; DP buffer bars show range line 2;
      the SD cache reopen after a reboot shows the graph at once (053 open item).

## Notes
- Remote-URL history fetch (051 device item) is checked when the user is on a hotspot; not part of this task.
