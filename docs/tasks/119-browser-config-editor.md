# 119: Edit config.yaml (and optionally secrets.yaml) from the web installer page

Status: PC part done, device checks pending (UNVERIFIED, docs/device-checks.md)
Depends on: 102 (web installer), 020 (provisioning protocol)
UI spec: docs/ui/config-editor.md (accepted 2026-11-05)
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
- User 2026-11-05: spec approved (docs/ui/config-editor.md). Secrets form adds an optional
  outside HA address (`external_url`, https) and an optional second Wi-Fi network (name/ssid/password, `ha_url` choice).
  Static IP and more networks: hand-written file. Keep the password 8-63 check and the "token looks short" warning.
- The protocol has only `putfile`: reading config.yaml back needs a new firmware read command (config.yaml only; never secrets).
- Done (firmware-dev, branch worktree-agent-a451140ef622f1221): `getfile config.yaml` (lib/provision parseGetfile, config.yaml only;
  tests refuse secrets.yaml and 16 other spellings), src/provision.cpp sendConfig; site/provision-client.js (+ record_frames.py ->
  frames.json from tools/provision.py, node test); site/config-check.js (config.yaml checker, secrets validation + YAML build);
  site/config-editor.js + install.html/style.css section; site/check.py security scan (no fetch/storage/console/location in the
  editor JS, secrets inputs autocomplete off and no name) and runs the node tests; FLASHER.md, guide getting-started.
- Decisions: the port stays open between Read and Write (closed on error, after a write, pagehide, install click); a write sends `reboot`;
  the key table in config-check.js is hand-kept from the reference page (config.cpp is being split, so no extraction from it);
  an entity with `type:` skips the domain check; secrets are emitted as JSON-quoted YAML scalars, empty password omitted.
- Open: not looked at in a browser (no DOM test), the page version line ("Inkhaus 1.5.0") is not shown (the protocol has no version command).
