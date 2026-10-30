# 061: Release workflow: firmware assets on GitHub Releases

Status: PC part done; GitHub run pending
Depends on: none
UI spec: none
Needs device: no

## Scope
GitHub Actions workflow: on a `v*` tag, build the m5paper release env, run native tests, and attach to a GitHub release:
bootloader.bin, partitions.bin, boot_app0.bin, firmware.bin and `manifest.json` (version, chip, flash size, per segment
offset/size/sha256). Version comes from include/version.h and must match the tag (fail otherwise).
Out of scope: the flasher binaries (063).

## Files to touch
- .github/workflows/release.yml, tools/make_manifest.py (+ a native/PC test for it)

## Acceptance criteria
- [x] `tools/make_manifest.py` produces a manifest matching the local `pio run` outputs; offsets match default_16MB.csv.
- [ ] Workflow validated locally (e.g. `act` or actionlint) and on GitHub after the user pushes a test tag.
- [x] Tag/version mismatch fails the job.

## Notes
- Never push; the user pushes tags.
- PC part done: workflow YAML parses (yaml.safe_load); manifest script dry-run against local `pio run` outputs matches
  default_16MB.csv (0x1000, 0x8000, 0xe000, 0x10000); tag mismatch exits 1 (script and workflow step). Test: `python3 tools/tests/test_make_manifest.py`.
- Pending: the GitHub run (user pushes a test tag, e.g. v1.0.0 matching include/version.h). actionlint/act were not available.
