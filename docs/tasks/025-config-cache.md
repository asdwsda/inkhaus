# 025: Config cache in internal flash

Status: in-progress
Depends on: 003, 020
UI spec: none
Needs device: yes (final timing check)

## Scope
Per ARCHITECTURE.md ("parsed only when the file changes", "SD for editing, internal flash for use"):
- On boot, stat `/config.yaml` on SD (size + mtime, or a CRC32 if mtime is unreliable on FAT); if it
  matches the cached fingerprint, load the parsed model from LittleFS (a compact binary or JSON
  serialisation of `config::Config`) without parsing YAML and without reading the whole SD file.
- If it changed (or no cache): parse, validate, write the cache atomically (tmp + rename), store
  the fingerprint. On parse errors keep the error path (no cache write).
- `putfile config.yaml` (task 020) invalidates the cache.
- Log `CONFIG: source=cache|sd|demo ms=…`. Never log values.
- Serialisation round-trip in lib/config (or a new lib) with tests: every field incl. timezone,
  groups, sizes, connection (the cache lives in internal flash; it holds the same secrets as the SD
  file, which is acceptable per ARCHITECTURE.md; never log it).

## Files to touch
- src/app_config.* (cache path), src/config_sd.* (stat), lib/config (serialise/deserialise + tests),
  src/provision.cpp (invalidate on write)

## Acceptance criteria
- [ ] Round-trip tests cover every config field; a corrupted or truncated cache file is rejected.
- [ ] `pio test -e native` and the builds pass.
- [ ] On device (later): the second boot logs `source=cache` and is faster than `source=sd`.

## Notes
- PC-only until the device is back. Don't touch src/main.cpp beyond a one-line call if unavoidable
  (task 024 edits main.cpp in parallel).
- PC part done (2026-10-28): lib/config config_cache (binary "MPCC" + CRC32, all fields; 7 tests incl.
  every truncation and bit flip), `config_sd::stat`, cache behind `app_config::load()` (LittleFS
  `/config.cache`, tmp + rename), invalidated by `putfile config.yaml`. Device timing check pending.
  Known limit: a PC edit keeping size and mtime identical isn't detected.
