# 115: Slow taps after waking on battery

Status: superseded by 116, 120
Depends on: 080, 093
UI spec: none
Needs device: yes

## Scope
User 2026-11-03 morning: on battery, after waking, taps took 5-10 s to act. Awake on USB they take 0.4-0.75 s (serial
capture, scratchpad slow/capture1.log). Find where the time goes after a wake: Wi-Fi connect/scan (056, 080), DNS, first
TCP/TLS, HA fetch queue ahead of the tap (state fetch, graphs, report 100, notifications 103) or the session state machine.
Use the SD wake log (tools/wakelog.py) for the morning sessions and fresh captures with Keep awake Off on battery-like
conditions.

## Acceptance criteria
- [ ] Root cause named with log evidence; fix as a follow-up task or here if small.
- [ ] Tap-to-action after a wake <= 2 s in 9 of 10 tries (log `timing` lines).
