# 054: WebSocket statistics client and parser (PC part of history phase 2)

Status: todo
Depends on: 050, 051
UI spec: docs/ui/history.md
Needs device: no (device wiring is task 055)

## Scope
Pure, native-tested code for `recorder/statistics_during_period` over a short-lived WebSocket
(046 research, part B; task split item 7). No Arduino code, no wiring into the popup (055).

1. **Minimal WebSocket client (RFC 6455 subset)** in lib/ha_client, over an abstract byte socket
   (`WsSocket`: write, read with timeout, close; the TLS connect lives in 055):
   - Upgrade request builder (`GET /api/websocket`, Host, random 16-byte key, version 13) and response
     check (101, `Sec-WebSocket-Accept` = base64(SHA-1(key + GUID))). Small SHA-1/base64 in lib (native).
   - Frame encoder: client-masked text, close, pong. Frame decoder, streaming: text/continuation
     (fragmented messages), ping -> pong, close, 16/64-bit lengths; a message over the byte limit aborts.
2. **HA protocol flow**: `auth_required` -> send `{"type":"auth","access_token":...}` -> `auth_ok`
   (`auth_invalid` = token error) -> send one command `id:1` -> stream the `result` message to the parser
   -> close frame. Errors: `success:false` (with HA's code), unexpected message, closed early.
   Bounded by `StreamLimits` (512 KB, 15 s deadline, cancel flag) like 051; maps to the same `HaError`s.
3. **Command builder**: statistic_ids = one entity, start/end ISO-8601 UTC, `period`
   (5minute | hour | day), `types` = only the ones needed (mean,min,max for line; the chosen statistic for bars).
   Period choice (pure function): line: hours <= 24 -> 5minute, <= 168 -> hour, else day;
   bar: config `period` (Auto: hour for <= 48 h, else day).
4. **Streaming statistics parser** in lib/history: scans the `result` array without a DOM; `start`/`end`
   as epoch ms (number) or ISO string; fills a `history::Series` with the existing column/bucket layout
   (line: each bucket's span sets its columns' min/max/last = mean; bars: one Bucket per stat bucket
   with the chosen statistic in `mean`/`last`; `change` per `Series::change`). Missing values = has=false.
   Empty result for the entity = no statistics (distinct outcome: "no statistics for this sensor").
5. **Source choice** (pure): `source: auto` = history for binary or hours <= 24, statistics otherwise;
   bar always statistics. Replaces `needsStatistics` in 055, not here.

Out of scope: TLS socket, net-task job, popup wiring, heap check (055).

## Files to touch
- lib/ha_client/src/ws_*.{h,cpp}, sha1/base64 helper, statistics command builder
- lib/history/src/stats_parser.{h,cpp}, source/period choice
- test/test_ws_client/, test/test_history_stats/

## Acceptance criteria
- [ ] Native tests: handshake accept check (RFC 6455 sample key), masked frame encoding, decoder with
      fragmented and split-across-reads frames, ping/pong, close, oversize abort.
- [ ] Native tests: full HA flow on a fake socket (auth_ok, auth_invalid, success:false, early close,
      deadline, cancel); the token never appears in any log/err string.
- [ ] Native tests: statistics parser for ms and ISO times, 24 h/5min, 7 d/hour, 30 d/day, missing
      values, empty result; series matches expected columns/buckets.
- [ ] `pio test -e native` and `pio run` pass.

## Notes
- Decision (architect, 2026-10-30): own minimal WS client over the existing TLS client instead of
  `esp_websocket_client`/arduinoWebSockets. Reason: native-testable, reuses the CA bundle and
  StreamLimits, no new dependency, we need only one command per connection.
