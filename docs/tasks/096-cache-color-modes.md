# 096: Keep supported_color_modes in the state cache

Status: done
Depends on: 045
UI spec: none
Needs device: no (PC part); a cached boot check is optional

## Scope
`EntityState::color_modes` (lib/model/src/entity_state.h) is not packed by the state codec, so after a
cached boot (RTC or flash record) a light falls back to the `brightness` attribute to decide whether it
is dimmable / which popup to show. Add `color_modes` to the codec record (lib/ state_codec), with a format
version bump so an old record is rejected cleanly (or read with empty color_modes; pick one and say which).
Out of scope: other attributes, cache file location, RTC/flash policy in src/state_cache.cpp.

## Files to touch
- the state codec in lib/ (state_codec.h/.cpp), its native test
- src/state_cache.cpp only if the record size check needs it (kRtcCap = 4096)

## Acceptance criteria
- [x] Native test: pack/unpack round-trips `color_modes` (empty, one mode, several modes).
- [x] Native test: an old-format record is handled as decided (rejected -> Source::None, or restored with empty color_modes); no crash, no garbage.
- [x] Size: record growth for a typical light is a few bytes; note the size of the demo/test config's record vs kRtcCap in Notes.
- [x] `pio test -e native` and `pio run` pass.

## Notes
- Decision: format 1 -> 2; old (v1) records are rejected (Source::None), not read with empty color_modes. Cache refills on the next fetch; one cold boot after the update shows no cached states.
- Added `w.str(color_modes)` after `status` (2 bytes when empty, +len for lights, e.g. 13 bytes for "color_temp,xy"). Typical record is ~100-200 B per entity (full-attr climate ~250 B); a dashboard of ~20 entities stays well under kRtcCap = 4096, and a larger blob already falls back to flash only (state_cache.cpp unchanged).
- Tests: test_model color_modes round-trip (empty/one/several), hand-built v1 record rejected. Native 795 pass, pio run OK.
