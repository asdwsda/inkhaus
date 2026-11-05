# 119: Edit config.yaml (and optionally secrets.yaml) from the web installer page

Status: todo
Depends on: 102 (web installer), 020 (provisioning protocol)
UI spec: docs/ui/experiments/config-editor.md (ui-designer, to be written)
Needs device: yes (read/write over Web Serial; PC part first)

## Scope
A "Device config" section on /install/ that talks to the device over Web Serial with the existing provisioning
protocol (lib/provision/src/provision_proto.*, tools/provision.py is the reference client):
- Read config.yaml from the device, edit it in a text area, check it before writing (YAML syntax + the known keys,
  reusing site/configkeys.py data as a JS table), write it back, device reloads.
- secrets.yaml is an optional, clearly separate step (user 2026-11-05): a notice that the page does not store,
  send or log the values; everything stays in the browser and goes only to the device over USB. Never read
  secrets back from the device (write-only), never put them in localStorage, URLs or console logs.
- No network requests with config or secrets content (the page stays static; check.py/a test asserts no fetch/XHR
  in the editor code path except the existing release files).
- Out of scope: Wi-Fi setup page / first-run hotspot (dropped by the user), remote (non-USB) editing.

## Files to touch
- site/install.html, site/install.js (or a new site/config-editor.js), site/style.css
- lib/provision only if the protocol lacks a read command (then device firmware + tests)
- docs/FLASHER.md, docs/guide (getting started: optional browser path)

## Acceptance criteria
- [ ] JS protocol client matches provision_proto framing (a node test with recorded frames from tools/provision.py).
- [ ] Secrets: write-only, no storage, notice shown; code review by tester confirms no leak path.
- [ ] `python3 site/build.py && python3 site/check.py` pass; page weight within the limit.
- [ ] Device check: read, edit, write config.yaml from Chrome; device reloads the new layout.

## Notes
- User 2026-11-05: spec approved (docs/ui/experiments/config-editor.md, moving to docs/ui/). Secrets form adds an optional
  outside HA address (`external_url`, https) and an optional second Wi-Fi network (name/ssid/password, `ha_url` choice).
  Static IP and more networks: hand-written file. Keep the password 8-63 check and the "token looks short" warning.
- The protocol has only `putfile`: reading config.yaml back needs a new firmware read command (config.yaml only; never secrets).
