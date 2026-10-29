# 050: History data core (lib/history)

Status: done
Depends on: 046
UI spec: none
Needs device: no

## Scope
Per 046 task split item 3: ISO-8601 to epoch, streaming scanner for the REST history JSON (minimal_response, no_attributes), downsampler (min/max/last per pixel column, carry-forward, binary vs numeric), bucket aggregation for bars, y-axis scaling and ticks, cache (de)serialisation with a params hash.

Research and design: 046-history-research.md (accepted by the user 2026-10-29).

## Files to touch
- lib/history (new), test/test_history (new)

## Acceptance criteria
- [ ] Native tests with generated 24 h / 7 d fixtures, unavailable/unknown values, empty `[]` and `[[]]`, chunked input split at every byte boundary.
- [ ] RAM use bounded (no full-body buffer); documented in the header.

## Notes
- 2026-10-29: done. Scanner/Collector/axis/cache in lib/history; time-axis ticks left for 052.
