# 033: Built-in demo showcase (demo.yaml)

Status: done
Depends on: 017, 028, 030
UI spec: docs/ui/widgets.md (incl. §11 wording, §12 details), STYLE.md, layout.md
Needs device: yes for screenshots (later)

## Scope
- New `demo.yaml` in the repo root (no connection, no secrets): a showcase dashboard, embedded in
  the firmware instead of config.example.yaml as the "no config.yaml" fallback. Tabs (suggested):
  "Living room" (realistic mix), "Sensors" (binary_sensor classes from §11, sensor with attribute +
  details at 2x1, a 1x1 sensor, weather), "Covers & vacuum" (single cover at several positions,
  a mixed cover group, vacuum in different states incl. low battery), "Climate" (two thermostats,
  heating/idle), plus enough tabs to trigger the tab overflow and one tab with enough widgets to
  show the pager. One deliberately unavailable widget.
- Demo states (lib/ui_state demo_states): fake states keyed by the entity ids in demo.yaml that cover
  every case above (device_class, positions, vacuum states/battery, attributes for the details line).
  Fake actions keep working (pending → confirmed); one entity id ending in `_fails` makes its tap
  end in the Failed state (to demo the failed badge).
- Keep demo mode rules: no secrets.yaml → fake states; no config.yaml → demo.yaml. The status bar shows
  "Demo".
- Native test: demo.yaml parses with 0 errors and 0 clamp reports in both orientations; every widget
  has a demo state (except the deliberately unavailable one).

## Files to touch
- demo.yaml (new), lib/ui_state/src/demo_states.*, test/test_ui_state (or a new test), platformio.ini
  (embed demo.yaml; keep the other embeds), src/app_config.cpp (only the embedded-fallback symbol)

## Acceptance criteria
- [ ] The tests above; `pio test -e native` and the builds pass.
- [ ] On device (later, m5paper-debug): screenshots of every demo tab for the README.

## Notes
- Task 032 runs in parallel and edits config.example.yaml, src/app_config.*, src/config_sd.*,
  lib/config; keep the app_config change to swapping the embedded symbol.
