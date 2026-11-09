# Ready indicator (task 120, experiment)

Status: proposal, not accepted. Mockup: `docs/ui/experiments/ready-indicator.svg` (PNG in `experiments/png/`).
Follows STYLE.md 5.1-5.3 and 10; no new region, no new colour, one new icon use (`hourglass`, S, already in the icon set).

## Purpose
After a wake the frozen frame shows `updated 14:05` + wifi icon, which looks ready. The status bar must say at once whether a
tap goes out now or waits. Entities: none new. Input is the net state (Wi-Fi up, HA reachable, fetch done) from task 116.
"Ready" = Wi-Fi up AND HA reachable (a command can be sent); the state fetch may still run.

## Where
The existing connection icon + label (portrait region (0, 0, 288, 54), landscape (16, 4, 144, 56), same hit area = tap
refreshes). Same rects, F18 INK, shape carries the state (icon), not grey. Nothing is drawn on scheduled (RTC) wakes.

## States (portrait label one line, x = 48, cy = 27; landscape two lines x = 56, cy = 21 / 43, max width 96)
| State | Icon (S 24, same slot) | Portrait | Landscape | Clock icon |
|---|---|---|---|---|
| Asleep | wifi (frozen last frame) | `updated 14:05` + sleep pill | `updated` / `14:05` + pill | as before |
| Connecting (not ready) | `wifi-join` | `not ready, connecting` | `not ready` / `connecting` | no |
| Connecting, tap queued | `hourglass` | `queued, connecting` | `queued` / `connecting` | no |
| Ready | `wifi` | `ready` | `ready` (one line, cy = 32) | no |
| Ready, fetch done | `wifi` | `updated 14:05` (existing) | `updated` / `14:05` | per 5.3 |
| Offline | `wifi-off` | `offline, last 14:05` (existing) | `offline` / `14:05` | yes |
| Offline, tap queued | `hourglass` | `offline, queued` | `queued` / `offline` | yes |

Reading rule: wifi arcs (full) = taps go out now; one arc or hourglass = taps wait; slashed = no link. The hourglass is the same
"pending" glyph used on widgets, so "queued" reads the same everywhere. Landscape `connecting` is 96 px wide F18: fits exactly
(max width 96, ends x = 152); if the device font measures wider, use `wait`.
Changes to 5.3: on a wake the `fetching states` phase is replaced by `ready` (fetch runs silently, ends in `updated`); a manual
refresh tap still shows `fetching states`. Wi-Fi and HA steps are one state (no separate `reaching HA`), to save refreshes.

## Interactions
- Tap on a widget while not ready (Connecting or Offline): existing pending look (requested state, 3 px dashed border, hourglass
  badge, STYLE 9) = partial 1, widget rect. The FIRST queued tap also switches icon+label to `queued, ...` = partial 2, status
  region. Further taps: widget partial only (label already says queued; a count would cost a refresh per tap and is not shown).
- Link becomes ready: label `ready`; the queued commands go out; widgets settle through the normal pending -> confirmed path.
- Never ready (timeout, task 116) -> Offline; queued taps stay pending (existing Failed rules apply when dropped).
- Tap on the label: refresh now (unchanged); ignored while Connecting. Rocker: same actions as today; a rocker action while not
  ready is queued exactly like a tap. Rocker wake (and touch wake) draw `not ready, connecting` at once; the waking tap is consumed.

## Refresh cost (all `epd_fastest`: the region holds only INK/PAPER; no greys, so no quality pass)
| Change | Region | Count |
|---|---|---|
| Wake (interactive) -> Connecting | portrait (0, 0, 412, 54) = status region + pill clear in ONE rect; landscape: status (16, 4, 144, 56) and slot 5 (706, 8, 108, 48), two partials (existing pill clear) | 1 (2 landscape) |
| Connecting -> Ready | status region | 1 |
| Ready -> `updated` (fetch done) | status region | 1 |
| Connecting -> Offline | status region | 1 |
| First queued tap | widget rect + status region | 2 |
| Later queued taps | widget rect | 1 |
Typical wake with fast Wi-Fi: 3 small partials (~130 ms each, region 288 x 54 = 3 % of the screen). Coalescing: a state shorter than
400 ms is not drawn (if Ready and fetch done come within 400 ms, draw `updated` directly) so a fast network costs 2. Never a full
refresh. Scheduled wakes: 0. The status bar has only INK text and icons, so this never needs GC16; after a full refresh the
region has no greys either.
Open for the architect: whether status-region partials (15.6 kpx) count toward the "12 feedback refreshes" ghost debt; recommend
count by area only (a wake adds ~9 % of a screen), otherwise every interactive wake burns 3 of 12.

## States
- Loading / no cache: icon and label as above; widget values `-`.
- Offline / HA unreachable: row `Offline`; Wi-Fi up but HA down is also Offline (not ready).
- Auth error: `lock` + `token rejected` (existing) is not ready; taps queue like offline.
- Demo: unchanged (`demo mode`, always ready).
- Entity unavailable: no change (unavailable widgets ignore taps, nothing queued).
