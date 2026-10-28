# 026: Follow-ups from device session 1

Status: in-progress
Depends on: 023
UI spec: docs/ui/widgets.md (sensor), STYLE.md (fonts)
Needs device: yes for the visual check (later)

## Scope
1. **Readable text states.** Non-numeric sensor values (e.g. `above_horizon`) are drawn in F24 like other
   widget text, in every sensor size (the wide layout currently uses F56, user: "looks really ugly").
   Humanise raw HA states for display: underscores → spaces, first letter capitalised
   (`above_horizon` → "Above horizon"); keep numbers and units untouched; keep the existing wording
   tables (On/Off, Open/Closed, cover/vacuum states) where they already apply. Pure function in
   lib/ui_state with tests.
2. **Remote URL first while away.** Persist `HaClient`'s remembered base URL across sleep and reboots
   (RTC memory + NVS, written only on change) and restore it at session start via
   `HaClient::set_remembered()`, so over the hotspot the external URL is tried first (saves the
   ~4 s local connect timeout per wake). Fall back automatically when it fails (existing logic).
3. **Nothing to fetch = success.** A config without entities must not produce `fetch_done err=Parse`:
   skip the fetch (or treat an empty id list as an OK result with 0 states) and show Online.

## Files to touch
- lib/ui_state (humanise + sensor view), src/render/widgets.cpp (sensor text font), tests
- lib/app_logic / src (remembered base persistence), lib/ha_client only if needed, tests
- lib/session or lib/app_logic (empty fetch), tests

## Acceptance criteria
- [ ] Native tests for: humanise, sensor text font choice (view level), remembered-base restore order,
      empty-fetch → Online. `pio test -e native` and the m5paper build pass.
- [ ] On device (later): "Above horizon" readable in F24; second wake over the hotspot tries the
      remote URL first (log `link=remote` and a shorter fetch); a config without entities shows Online.

## Notes
