# 136: First service call hangs 8 s and fails all queued taps

Status: done (PC; device check open)
Depends on: 126, 130
UI spec: none
Needs device: no (device check later)

## Scope
Device log 2026-11-27 (rc1 debug), scratchpad copy in docs/tasks/assets/136-checks.txt:
- Wake 1 (t=101.95) and wake 2 (t=232.17): the first call `light.turn_on light.shellydimmer2_...` opened a new
  socket (`reused=0`, tcp 15-22 ms) though a fetch finished ~3 s earlier, then got no response:
  `ttfb=8021`, `call_done err=2 http=0`. Status went `offline` and all 4 tapped widgets were marked failed
  (`cmd mark=2 ids=4`). The 3 later taps were never sent.
- Wake 3 (t=266): the same calls all succeeded, 50-230 ms each on a reused socket.
1. Find why the call socket was new and why HA did not answer (compare with wake 3; look at the keep-alive
   transport and what closes the fetch connection; check whether the request was fully written).
2. A call timeout must not fail taps that were never sent: send them next (they are `not_sent`), and do
   not show `offline` while Wi-Fi is up and a later request succeeds.
3. Consider a shorter first-byte timeout for calls (e.g. 4 s), keeping the 130 no-double-send rules.

## Files to touch
- lib/session, lib/ha_client (keep_alive, ha_client), src/ha_transport_arduino.cpp, src/app_*.cpp, tests

## Acceptance criteria
- [ ] Cause of the 8 s stall stated in Notes (or the best evidence-based guess plus a log line that settles it).
- [ ] Native test: call 1 times out, queued calls 2..4 are still sent; only call 1's widget shows failed.
- [ ] No double send of a call that may have reached HA (130 tests still pass).
- [ ] `pio test -e native` and `pio run` pass.

## Notes

Cause (UNVERIFIED, from docs/tasks/assets/136-checks.txt): in wakes 1/2 the first fetch ended at 229.37, then the
background history-stats stream (type 3) started on the shared WiFiClient. `Raw::stream` uses `setReuse(false)`,
so its `http_.end()` closes the keep-alive socket. A tap at 232.37 made the stats stream yield (`net_yield`), so
the call found no open socket and connected anew (`reused=0`, tcp 15 ms, request written). HA did not answer for
8 s; the requeued stats query then took 9.7 s (364 ms in wake 3), so HA was most likely busy (stats/template
work, perhaps the aborted stream's query), not the socket. Wake 3 had no stream between fetch and call, so the
socket was reused. The 8 s read timeout then ended the call as `Network`, which `SessionMachine` treated as
terminal (Offline + all queued marked failed).
Changes: session: a call `Network` error fails only that call (`call_net_fails_`), queued calls still go out, a
refresh fetch follows, Offline only on a 2nd consecutive call failure or a failed fetch. Transport: service-call
POSTs use a 4 s read timeout; new log `NETT: new_conn why=first|drop|stream call=<0/1> code=<n>` settles the
socket cause. No-double-send rules (130) untouched. Possible later fix: give streams their own WiFiClient.
Review fixes: retry after Offline resets `call_net_fails_` (native test added); the 4 s read timeout applies only to plain
light/switch/fan/input_boolean/lock/scene calls (not return_response, not other domains); `g_closeWhy` reset after the
new_conn log.
