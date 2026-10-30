# 064: Flasher: optional YAML upload to the SD card

Status: done
Depends on: 062, 020
UI spec: in this file
Needs device: yes

## Scope
Secondary "Upload YAML files..." button: pick config.yaml and/or secrets.yaml, send each to the SD card with the task 020
serial protocol (port of tools/provision.py: size + CRC32), then offer a restart. No editing, no wizard.
The tool never logs file contents (secrets).

## Files to touch
- tools/flasher/

## Acceptance criteria
- [x] Unit tests for the protocol framing against provision.py's format.
- [x] Device: a test-*.yaml upload round-trips (CRC confirmed by the device); file contents never appear in logs.
- Device check 2026-10-30 (user): install with progress, settings kept, YAML upload + automatic restart OK.
