# 046: History popup for sensors (research)

Status: todo
Depends on: 030
UI spec: later (after the research)
Needs device: no for the research

## Goal (user request, 2026-10-30)
A popup on sensor widgets (long press / rocker long-push) showing historical data as a graph, with the graph
type configurable in config.yaml. Research first; write findings + a recommendation into this file.

## Questions
1. **Data source:** REST `GET /api/history/period/<start>?filter_entity_id=…&end_time=…&minimal_response&
   no_attributes&significant_changes_only` vs long-term statistics (`recorder/statistics_during_period`,
   WebSocket only; hourly mean/min/max, 5-min short-term). Sizes for 24 h / 7 d of a typical temperature
   sensor; response time via the user's remote URL (Cloudflare); fit within the 64 KB body cap or streaming.
2. **On-device processing:** downsampling to the graph width (min/max per pixel column), memory (PSRAM),
   time; numeric vs non-numeric (binary_sensor → step/"on" bands).
3. **Graph types worth supporting on e-ink:** line (with min/max band), bar (e.g. daily energy), step
   (binary/on-off timeline); greys and refresh mode (quality refresh for the popup).
4. **Config schema proposal:** e.g. per widget `history: { hours: 24, type: line|bar|step, min, max,
   statistic: mean|max|sum }`, defaults when absent (long press shows a 24 h line for numeric sensors).
5. **Caching and battery:** fetch only when the popup opens; cache last graph per entity (RTC/LittleFS) to
   show instantly while refreshing; cost estimate per open.
6. **Server-side rendering fallback** (ARCHITECTURE.md decision): is on-device clearly sufficient?

## Deliverable
Findings with numbers, a recommended design (data source, graph types, config keys), and the task split
(ui-designer spec, firmware tasks). No code.
