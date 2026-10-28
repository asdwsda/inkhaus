# 032: Separate secrets.yaml from the dashboard config.yaml

Status: done
Depends on: 003, 020, 025
UI spec: none
Needs device: yes for the final check (later)

## Scope
Per ARCHITECTURE.md (2026-10-28):
- lib/config: a `parseSecrets()` for `/secrets.yaml` with exactly the old `connection:` structure
  (`wifi:` + `home_assistant:`, top level, no `connection:` wrapper), validated with path + line,
  messages never echo values. `Config` keeps one `connection` field, merged from secrets.yaml.
- Loading (src/app_config, src/config_sd): read both files. secrets.yaml present → its connection is
  used, and a `connection:` in config.yaml is an error ("move it to secrets.yaml"). No secrets.yaml →
  a `connection:` in config.yaml still works, with `CONFIG: warning=connection_in_config`. Neither →
  demo mode (as today).
- Config cache: the fingerprint covers both files (bump the cache format); still stored in internal flash.
- Provisioning: `putfile` whitelist gains `secrets.yaml`; writing either file invalidates the cache.
  tools/provision.py: `--name secrets.yaml` works; the summary for secrets.yaml is only
  `PUT-SECRETS ok wifi=yes ha=yes` (never values).
- Files and docs: `secrets.example.yaml` in the repo root (fake values), `config.example.yaml` loses
  its `connection:` section (comment pointing to secrets.example.yaml), `.gitignore` adds
  `secrets.yaml`, README + CLAUDE.md + the m5paper-device skill: the rule "never read" now applies to
  `secrets.yaml` (and still `include/secrets.h`); `config.yaml` is allowed to be read when the user
  shares it.

## Files to touch
- lib/config (+ tests), src/app_config.*, src/config_sd.*, src/provision.cpp, tools/provision.py,
  config.example.yaml, secrets.example.yaml (new), .gitignore, README.md

## Acceptance criteria
- [ ] Native tests: secrets parsing + errors (no value echo), merge rules (secrets wins, connection in
      config rejected when secrets exist, transition warning), cache fingerprint over both files.
- [ ] tools/tests/test_provision.py covers `--name secrets.yaml` with a fake device.
- [ ] `pio test -e native` and the builds pass.
- [ ] On device (later): split files provisioned, `CONFIG: source=sd` then `cache`, real mode works.

## Notes
- The architect updates CLAUDE.md and the skill (agent rules) after review.
