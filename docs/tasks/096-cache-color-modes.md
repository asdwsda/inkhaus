# 096: Keep supported_color_modes in the state cache

Status: in-progress
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
- [ ] Native test: pack/unpack round-trips `color_modes` (empty, one mode, several modes).
- [ ] Native test: an old-format record is handled as decided (rejected -> Source::None, or restored with empty color_modes); no crash, no garbage.
- [ ] Size: record growth for a typical light is a few bytes; note the size of the demo/test config's record vs kRtcCap in Notes.
- [ ] `pio test -e native` and `pio run` pass.

## Notes
