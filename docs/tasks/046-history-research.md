# 046: History popup for sensors (research)

Status: done (research; design accepted by the user 2026-10-29; build tasks 048-054)
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


## Findings (research, 2026-10-29)
All numbers marked (est.) are my estimates from HA's documented JSON shapes and typical ESP32 behaviour, not
measurements. No call was made to any HA instance. Reviewed: ARCHITECTURE.md (HA connection, decisions of 10-27/10-29),
`lib/ha_client/src/body_limit.h` (64 KB body cap `kMaxResponseBody`, 15 s total request deadline, `BodySink`),
`src/ha_transport_arduino.*` (HTTPClient + `CappedStream`, `setReuse(true)`, connect 4 s / read 8 s timeouts, one
keep-alive connection per wake), `lib/config/src/config_model.h` (`Widget` has `attribute/details/unit/decimals`; no history key yet).

### 1. Data source
**A. REST `GET /api/history/period/<start>?filter_entity_id=<id>&end_time=<end>&minimal_response&no_attributes&significant_changes_only`**
- Returns `[[ {first state object}, {"last_changed":"2026-10-29T12:34:56.123456+00:00","state":"21.4"}, ... ]]`
  (with `minimal_response` only the first entry carries entity_id; later ones carry `last_changed` and `state`; with
  `no_attributes` the attributes are dropped, including from the first entry). About 70-75 bytes per point (est.).
- One point per recorded state change, NOT per sample interval. `significant_changes_only` only drops attribute-only
  changes (it keeps every state change for sensors), so it does not thin out a noisy numeric sensor. Size therefore depends on the sensor:

| Sensor type | Points/24 h (est.) | 24 h body (est.) | 7 d body (est.) |
|---|---|---|---|
| Room temperature, changes every ~2-5 min | 300-700 | 22-50 KB | 150-350 KB |
| Humidity / illuminance, every ~1 min | ~1,500 | ~110 KB | ~750 KB |
| Power meter, every 5-10 s | 9k-17k | 0.6-1.2 MB | 4-8 MB |
| binary_sensor / switch (door, light) | 5-100 | < 8 KB | < 60 KB |

  So 24 h of a typical temperature sensor fits the 64 KB cap only some of the time; 7 d does not; noisy sensors never.
- Server time (est.): 24 h of one entity 50-300 ms on a Pi-class host (recorder DB query + JSON), 7 d 0.3-1.5 s.
- Latency via the remote URL (Cloudflare, HTTPS) (est.): new TLS handshake 0.8-1.5 s on the ESP32 (already paid
  once per wake; the connection is kept alive, so a popup opened in an active session pays 0 for it), request RTT 60-150 ms, then
  transfer at ~100-200 KB/s TLS on the ESP32 (est.): 50 KB = 0.3-0.5 s, 300 KB = 2-3 s. Cloudflare only compresses if the
  client sends `Accept-Encoding`; HTTPClient does not, and we have no inflate path, so the body arrives uncompressed.
  Total typical 24 h popup over the internet: ~1-2 s on a warm connection, 2-4 s cold (est.). Over LAN http: 0.3-0.8 s.
- **Cap/streaming:** the 64 KB cap is a safety net for the state fetch. History must not raise it globally. Needs a second
  transport mode: stream the body into a consumer (a sink callback that feeds the incremental parser, no `std::string` of
  the whole body), with its own byte limit (proposal 512 KB, ~4-5 s) and the existing 15 s deadline. When the limit is hit,
  stop, keep the points parsed so far (they are the oldest, so the graph would end early) and show "too dense, shown partial", or better abort and
  fall back to B. `CappedStream`/`BodySink` already sit at the right place; the cap becomes a parameter and a streaming
  `BodySink` variant is added.

**B. Long-term statistics, WebSocket `recorder/statistics_during_period`**
- Command: `{"id":N,"type":"recorder/statistics_during_period","start_time":"<ISO>","end_time":"<ISO>","statistic_ids":["sensor.x"],"period":"5minute|hour|day|week|month","types":["mean","min","max","sum","change","state"]}`
  after the handshake `auth_required` -> `{"type":"auth","access_token":...}` -> `auth_ok`. Result:
  `{"id":N,"type":"result","success":true,"result":{"sensor.x":[{"start":1761700000000,"end":1761703600000,"mean":21.4,"min":20.9,"max":21.8}, ...]}}`
  (start/end are epoch ms in current HA; older versions used ISO strings, so parse both or require HA >= 2022.x).
- Size: ~85-100 bytes per bucket (est.). 24 h hourly = 24 buckets = ~2.3 KB; 7 d hourly = 168 = ~15 KB; 30 d daily = 30 = ~3 KB;
  24 h at 5 min = 288 = ~27 KB. Always fits the 64 KB cap, independent of sensor noise. Mean/min/max come for free (the min/max band).
- Limits: only entities with `state_class` (measurement/total/total_increasing) have statistics; binary_sensor, switch, climate do not.
  5-minute data is kept `purge_keep_days` (default 10 d), hourly forever. Statistics lag: the current hour is only complete after ~5 min
  (short-term stats are written every 5 min), so the newest edge is up to ~5 min old; fine for a graph.
- Cost: no WS client in the current stack (HTTPClient has none; add `esp_websocket_client` from ESP-IDF or arduinoWebSockets). A WS is a separate
  TLS connection (cannot share the HTTPClient keep-alive socket): +1 handshake 0.8-1.5 s, upgrade + auth ~3 round trips (est. +0.3-0.5 s),
  query 50-300 ms. Memory for a second TLS session ~40-60 KB internal heap (est.; check the heap before; close the keep-alive socket first
  if tight). Total popup ~2-3.5 s (est.). Short-lived (open, one command, close), so it respects the "no persistent socket while idle" decision.
- There is no REST endpoint for statistics, and `POST /api/template` cannot read the recorder (no statistics function), so B needs WS.

**Verdict:** REST history (A) is the only source for binary/non-statistic entities and for short ranges (<= 24-48 h) of slow sensors; statistics (B) is
the only sane source for 7 d+, daily bars, and noisy sensors. A is available with no new stack (one new streaming mode); B needs a WS client.
Build A first, B second.

### 2. On-device processing
- Chart plot area (est.): portrait popup ~480x300 px, landscape ~800x320 px, so W = 480-800 columns. 24 h / 700 columns = ~2 min per column; 7 d = ~14 min.
- Streaming parse (no DOM): a small state machine scans for `"last_changed":"..."` and `"state":"..."`, converts ISO-8601 (fraction and offset) to
  epoch seconds (own routine, ~30 lines, no `strptime`/`mktime` TZ issues; HA sends UTC `+00:00`), `strtof` on the state; skips `unavailable`/`unknown`.
  ArduinoJson with a streaming filter would also work but allocates per object; the hand scanner is smaller and unit-testable natively.
- Downsample while parsing: per column keep `min, max, last` (3 floats = 12 B) -> 700 columns = 8.4 KB. Empty columns carry the previous value forward
  (HA history is a step function: a value holds until the next change), plus the value at `start` from the first entry. Memory: < 10 KB plus
  parser state; PSRAM not needed (the 0.5 MB canvas is already in PSRAM). Worst case if raw points were kept: 17k x 8 B = 136 KB (so do not keep them).
- Time (est.): scanning at ~0.5-1 MB/s on the 240 MHz core = 0.1-0.5 s for 50-300 KB, hidden by the network transfer. Drawing 700 columns of vertical lines
  + axes into the canvas: < 30 ms (est.). The network task already runs separately from the UI (ARCHITECTURE decision of 10-27), so the parse runs there.
- Non-numeric: the first parseable `state` decides. Numeric -> line/bar. `on/off` (also `open/closed`, `home/not_home`, `true/false`) -> step/bands:
  per column "fraction of time on" is overkill, so use `last` + any-change flag, drawn as filled bands. Other strings (climate hvac_mode, media_player): not supported in v1, popup says "No graph for this entity type".
- Attribute widgets (`attribute: temperature`): `no_attributes` removes the data, and keeping attributes costs ~300-600 B per point (5-10x), so v1
  graphs the entity state only; for widgets that show an attribute, history is disabled unless `history.entity` is set later. Keep it out of v1.
- Numeric axis: min/max from the data (padded 5 %) unless `min`/`max` are configured; 3 horizontal ticks, 4-5 time ticks (local time using the
  configured POSIX TZ). Fonts: DejaVu18/24 only, ASCII only (no degree sign glyph, per the font memory: write "C" or use `unit` text).

### 3. Graph types on e-ink
- **Line + min/max band:** the standard for a numeric sensor. Per column draw a 1 px vertical min..max bar in light grey (the band), the `last` value connected by a 2 px black polyline.
  With 2-minute columns the band is mostly invisible for slow sensors and visible for noisy ones. Good default.
- **Bar:** per bucket (hour/day) mean, max, or sum/change; 4-8 px wide black bars (7 d daily = 7 bars, 24 h hourly = 24). Needs bucket aggregates, so source B (`sum`/`change`
  for energy counters). With A, only approximate: last value per local day minus the previous (works for `total_increasing`, wrong across resets). Bars only really make sense with B.
- **Step / timeline:** binary entities as a lane: filled black band while on, white while off, ticks on the time axis. Source A.
- Greys: 16-level greys work in GC16 only. Use at most 3 shades: black (data), mid grey 0x80 (band/bars of secondary stat), light grey 0xD0 (grid lines, band). Avoid dithering/patterns.
- Refresh: a popup is an infrequent, user-initiated, full-attention view, so render the whole popup rect once with `epd_quality` (GC16, ~0.9 s for a larger box, per the device skill table), after
  the data has arrived. Show an immediate empty frame with a "Loading" text via `epd_fastest` black/white (~150 ms, no greys) on open for instant feedback, then one quality refresh when the
  graph is ready (cached graph: skip the loading frame and show it directly, see 5). Close: restore the underlying screen with the existing region redraw (quality refresh of the popup rect, to clear the greys).
  No timed redraws, and a graph never refreshes by itself while open.

### 4. Config schema proposal
Per widget (sensor, and any widget whose entity is numeric or binary); absent key = defaults; `history: false` disables the popup for that widget.
```yaml
- type: sensor
  entity: sensor.living_temperature
  history:            # optional; the whole block may be omitted
    hours: 24         # 1..720, default 24 (24 h; 168 = 7 d)
    type: auto        # auto | line | bar | step; auto = line for numeric, step for on/off
    statistic: mean   # mean | min | max | sum | change (bar and statistics source only), default mean (sum for energy device_class)
    period: hour      # bar bucket: hour | day; default hour for <= 48 h, else day
    min: 15           # optional fixed y-axis bounds; default autoscale
    max: 30
    source: auto      # auto | history | statistics; auto = history for hours <= 24 or binary, statistics otherwise
- type: sensor
  entity: sensor.today_energy
  history: { hours: 168, type: bar, statistic: change, period: day }
- type: sensor
  entity: binary_sensor.front_door
  history: 48       # shorthand: hours only
```
- Parse in `lib/config` (`Widget::history` optional struct with enums + validation: hours range, bounds `min < max`, type/source valid, unknown key = error with path
  as today), unit tests native. With `source: statistics` or `type: bar` before the WS task exists, the popup shows "not available yet" instead of failing config.
- Global (settings or config.yaml top-level) only `history_enabled` (default true) is needed; no per-tab keys.
- Trigger: long press on the widget (>= 600 ms, no service call is made; short tap behaviour unchanged) and a rocker long-push on the focused widget; closes with
  the same gesture, a tap outside, or the rocker. Check what task 030/input already provides for long-press before the firmware task.

### 5. Caching and battery
- Fetch only when the popup opens; never in the periodic wake state fetch.
- Cache per entity+params: header (entity, start/end epoch, hours, type, statistic, width, fetch epoch) + the downsampled columns (min/max/last as int16 scaled, 6 B x 700 = 4.2 KB,
  or 3 B/col if stored at 12-bit; for bars 24-30 buckets, < 200 B). Memory cache in PSRAM for the session (a few KB per entity), and on SD (`/cache/hist/<hash>.bin`, SD is already
  present and read for config; one 4 KB file per opened entity, writes are rare, so wear is a non-issue; internal flash/LittleFS also works but SD avoids partition work).
  RTC memory (8 KB slow) is too small and shared with other state. No cache is shown if the params hash differs (config edit).
- Open flow: cache younger than the freshness window (proposal: max(10 min, hours x 1 %) i.e. 15 min for 24 h) -> show instantly with "updated HH:MM" and no fetch;
  older cache -> show instantly (grey "stale" note), fetch, redraw only if the data changed (compare columns; avoids a second 0.9 s refresh), else just update the timestamp.
- Cost per open with a network fetch (est.): radio active 1-2 s warm (3-4 s if the connection had to be re-established), ~120-150 mA average for the ESP32 with Wi-Fi + TLS
  = ~0.04-0.15 mAh (est.), plus the e-ink refresh ~0.9 s at the panel's peak (est. < 0.03 mAh). Battery 1150 mAh -> ~0.01 % per open. The real cost is keeping the device
  awake while the user reads the graph (idle timer, 120 s hard cap from the architecture); the popup should use the normal idle timeout, not extend it.
- The in-flight request must be abortable (user closes the popup): the net task needs a cancel flag checked in the sink, so a closed popup stops the transfer.

### 6. Server-side rendering fallback
Not needed. Source B transfers 2-30 KB, source A (guarded by a byte limit) 30-300 KB; both fit the heap/PSRAM budget and parse time, and total popup latency stays 1-4 s (est.),
comparable to a PNG/BMP download of the same size (a 700x300 8-bit grey PNG is also tens of KB, and would need an HA-side renderer, extra setup and a second
URL/auth path). On-device keeps the graph styled like the rest of the UI and offline-capable from the cache. Revisit only if: measured p95 open time > 5 s over the remote URL,
multi-entity overlays or > 30 d ranges are wanted, or noisy sensors cannot be served by B. The ARCHITECTURE.md decision (device first) stands.

## Recommended design
- **Data source:** two-tier, `source: auto`. Phase 1 = REST history (`minimal_response&no_attributes&significant_changes_only`, one entity, streaming parse, 512 KB / 15 s limit) for
  binary entities and numeric `hours <= 24`; on limit hit show partial or an error line "too many points, set source: statistics". Phase 2 = short-lived WebSocket
  `recorder/statistics_during_period` (hour/day/5minute) for `hours > 24`, bars and noisy sensors.
- **Graph types:** `line` (with min/max band) default for numeric; `step` default for on/off; `bar` (needs statistics). Greys: black, 0x80, 0xD0 only; one quality refresh.
- **Config keys:** `history: { hours: 24, type: auto, statistic: mean, period: auto, min, max, source: auto }` plus shorthand `history: <hours>` and `history: false`.
  Defaults as above; long press on any sensor widget with a numeric/on-off entity opens a 24 h graph even without a `history:` block.
- **Caching:** PSRAM per session + SD file per entity (~4 KB, downsampled columns), fresh window ~15 min for 24 h, show-stale-then-refresh with a changed-only redraw.
- **Constraints to respect:** body cap stays 64 KB for normal calls; abortable fetch; no timers redrawing; popup closes back to the exact underlying screen via the canvas diff.

## Task split (numbers to be assigned by the architect)
1. **ui-designer:** popup spec + mockups (portrait and landscape): geometry, title/range line, axes and tick labels, line/band, bar, step lane, "Loading", "No data", "Too many points",
   "Not available" states, stale note, close gesture, and a hint of which long-press/rocker gesture opens it. Cover grey values and the refresh sequence of section 3. Needs only this file.
2. **fw-A `history` config (PC-only):** `Widget::history` struct, parsing of the block/shorthand/`false`, validation, example in config docs, native tests. Parallelisable now.
3. **fw-B `lib/history` core (PC-only):** ISO-8601 to epoch, streaming scanner for the history JSON, downsampler (min/max/last per column, carry-forward, binary/numeric detection),
   bucket aggregation for bars, y-axis scaling, cache (de)serialisation + params hash; native tests with generated 24 h / 7 d fixtures, incl. the response shape for `unavailable`, empty `[]` and `[[]]`.
4. **fw-C streaming transport (PC-testable + device):** a streaming request in `ha_client`/`HttpTransport` with sink callback, per-request byte limit and cancel flag; the
   `ha_transport_arduino` implementation (no 64 KB cap for that mode, deadline kept); native tests with fake transport. Net task job type "history fetch".
5. **fw-D graph renderer (PC + screenshot compare):** draws line/band/bar/step into the canvas from the downsampled data per the UI spec; screenshot check vs mockup. Needs 1 and 3.
6. **fw-E popup UI + input + cache + refresh sequence (device):** long press / rocker long push, popup state machine, SD+PSRAM cache, open/close flows, quality refresh. Needs 1-5. Device checks: open time over LAN and remote URL
   (log `HIST: fetch_ms parse_ms bytes points`), popup refresh look, close restores the screen without ghosts.
7. **fw-F (later) WebSocket statistics source:** client (auth, one command, close), parsing, `source: statistics`, bars, 7 d/30 d. Includes a heap check for the second TLS session.
Dependencies: 2, 3, 4 in parallel with the UI spec; then 5; then 6; 7 independent after 3.
