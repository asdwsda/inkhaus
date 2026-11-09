# 097: Set the clock from HA's HTTP Date header

Status: shipped v1.4.0, device check pending
Depends on: 008, 024
UI spec: none
Needs device: yes (short check later; PC part first)

## Scope
Every HA reply carries `Date:` (RFC 7231 IMF-fixdate, UTC, 1 s resolution). Use it as a time source so
the separate SNTP round trip is rarely needed (ARCHITECTURE 2026-10-28 "option for later").
- Pure parser in lib/ (e.g. `lib/app_logic/src/http_date.{h,cpp}`): `Sun, 06 Nov 1994 08:49:37 GMT` -> epoch;
  reject anything else (obsolete formats, non-GMT, out-of-range fields), no exceptions/allocation.
- Accept rule (pure, in lib/app_logic next to `shouldSyncNtp`): use a Date value only if the request
  round trip was <= 2 s and the HTTP status is 2xx; estimate = date + 0.5 s + RTT/2 (document it).
  Write the RTC only when the clock is untrusted or the estimate differs from the RTC by >= 2 s.
  An accepted Date counts as a sync for the NTP policy (store the last-sync time like SNTP does), so
  `shouldSyncNtp` returns false for 24 h. SNTP stays the fallback when no valid Date arrives.
- Capture the header in the existing HA client path (the `/api/template` state fetch is enough);
  wire into src/session_glue.* so RTC/NVS writes happen the same way as ntpPoll.
- Out of scope: changing the NTP 24 h policy itself; the WebSocket stats path.

## Files to touch
- lib/app_logic/src/ (new parser + accept rule), the HA HTTP client (header capture)
- src/session_glue.{h,cpp}
- test/ (new suite or the app_logic suite)

## Acceptance criteria
- [ ] Native tests: valid IMF-fixdate parses (incl. leap day, 1 Jan 2000, year 2038+); rejects RFC 850,
      asctime, non-GMT, bad month/day/time, truncated, empty.
- [ ] Native tests for the accept rule: RTT > 2 s rejected, non-2xx rejected, untrusted clock always
      written, < 2 s difference not written, >= 2 s written, accepted Date suppresses `shouldSyncNtp` for 24 h.
- [ ] `pio test -e native` and `pio run` (all envs) pass.
- [ ] Log line on accept, e.g. `TIME: http_date delta_s=N wrote=0|1`.
- [ ] Device-check line appended to docs/device-checks.md (UNVERIFIED tags in code where unsure).

## Notes
Decision: HA's clock is trusted like an NTP server (HA hosts are normally NTP-synced); error <= ~1.5 s,
fine for the minute-resolution UI.
