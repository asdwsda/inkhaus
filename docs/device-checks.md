# Device checks (pending verification)

Things built and unit-tested on the PC but not yet confirmed on the real M5Paper. Work through this list
with the device; tick items, and turn failures into tasks. Agents add every unverified assumption here,
tagged `UNVERIFIED` in code comments and task notes.

## Power and sleep
- [ ] Scheduled refresh (e.g. 30 min): the device wakes on the timer, fetches, redraws only changes,
      sleeps immediately (check with Debug log on: `/debug.log`, `/wakelog.csv`). Task 024/035.
- [ ] Wake modes: Button = rocker only, Touch = touch only, Touch & Button = both; indicator matches.
      Task 039.
- [ ] Standby current with the touch controller awake (USB-C meter / battery test). Task 035.
- [ ] Multi-day battery test with Debug log on. docs/battery-test-plan.md.
- [ ] Low-battery notice and sleep without alarm (temporarily raise the threshold). Task 024.

## Display
- [ ] Splash on a cold boot (after flashing; and on every reset once 1.0.1 lands). Task 041.
- [ ] Status bar states: connecting, fetching, updated, outdated (clock), offline, token rejected, demo.
      Task 038.
- [ ] "-" after the first fetch (once seen, never reproduced): watch `APP: fetch_applied`. Task 023.
- [ ] Settings value fit ("Touch & Button" in landscape). Fixed 85120f7.
- [ ] Landscape 4-column tabs match docs/ui/png/proposal-landscape-4col.png. Task 043.
- [ ] New state wording icons (door, motion, …) look right. Task 028.

## Home Assistant
- [ ] Covers with `unknown` state: normal look, "-", Opening/Closing/Stopped on tap, shutter moves. Task 040.
- [ ] Timing of the first HTTPS request vs reused ones (`HTTP: ms=… reused=…`). Task 036/027.
- [ ] Rapid taps on two widgets: both reach HA in order (measure only; no change planned).
- [ ] Remote-URL-first after a hotspot wake (`link=remote`). Task 026.

## Settings and tools
- [ ] Tools page: Demo mode on/off reloads; Debug log on creates `/debug.log` + `/wakelog.csv`; "No SD card"
      note without a card. Task 041/034.
- [ ] README statements: rocker behaviour description; Settings shows the first config error.

## Added by later tasks
<!-- agents append here: "- [ ] <what> (task NNN, UNVERIFIED: <assumption>)" -->
