# 122: Detect garage doors from device_class

Status: todo
Depends on: 108
UI spec: none
Needs device: no

## Scope
A `cover` entity with `device_class: garage` (or `gate`) becomes a garage widget without `type: garage`. The device
class is only known from HA state, not from config: map at the first state fetch (and cache it), keep the cover
widget until then; an explicit `type:` always wins. Same confirm defaults as task 108.

## Files to touch
- lib/config or lib/ui_state (widget kind resolution), lib/ha_client template if device_class is not fetched, test/

## Acceptance criteria
- [ ] Native tests: cover + device_class garage/gate -> garage; explicit type wins; unknown class stays cover.
- [ ] `pio test -e native`, `pio run` (all envs) pass; docs/guide 04 + 09 updated.
