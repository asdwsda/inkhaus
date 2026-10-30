# 056: Wi-Fi scan always fails (aps=-2 after 6 s)

Status: todo
Depends on: 047
UI spec: none
Needs device: yes

## Scope
Every cold boot in Auto with 2 profiles logs `NET: phase=scan profiles=2` then
`NET: scan_done aps=-2 found=0 pick=0 ms=6004` (WIFI_SCAN_FAILED, hits the 6 s budget); Auto only works
because it falls back to profile 0's cache. One earlier boot failed entirely (`attempt_scan`,
`failed connect_ms=8010`). Evidence: scratchpad visual.log t=5.2-11.2 (session 2026-10-30).
Find why the async scan (`WiFi.scanNetworks(true,false,false,300)`, src/net.cpp) fails (mode/STA not
started, scan started before `WiFi.mode`, polling the result wrongly, ...) and fix it.
Out of scope: profile selection logic (lib), unless the bug is there.

## Files to touch
- src/net.cpp (and lib net selection only if needed), tests

## Acceptance criteria
- [ ] Root cause in Notes.
- [ ] Device: cold boot in Auto logs `scan_done aps>0 found>=1` well under 6 s and connects to the
      strongest listed profile in range.
- [ ] Forced profile and the cached path unchanged (`cache=hit`, ~0.3 s).

## Notes
