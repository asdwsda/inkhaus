#!/usr/bin/env python3
"""Regenerate tools/testdata/webflash (a fake `gh api releases` list + tiny release folders). Task 102."""
import json
import os

base = os.path.join(os.path.dirname(os.path.abspath(__file__)), "webflash")
api = []


def rel(tag, date, pre=False, man=True):
    api.append({"tag_name": tag, "published_at": date, "prerelease": pre, "draft": False,
                "assets": [{"name": "manifest.json" if man else "firmware.bin"}]})


rel("v1.3.1", "2026-11-02T14:46:15Z")
rel("v1.3.0", "2026-11-02T13:30:06Z")
rel("v1.2.1", "2026-10-30T16:45:21Z")
rel("v1.4.0-rc1", "2026-11-03T08:00:00Z", pre=True)
rel("v1.2.0-rc1", "2026-10-01T08:00:00Z", pre=True)
rel("v1.1.0", "2026-10-20T00:00:00Z", man=False)
os.makedirs(base, exist_ok=True)
with open(os.path.join(base, "api.json"), "w") as f:
    json.dump(api, f, indent=1)
for tag, ver in [("v1.3.1", "1.3.1"), ("v1.3.0", "1.3.0"), ("v1.2.1", "1.2.1"), ("v1.4.0-rc1", "1.4.0-rc1")]:
    d = os.path.join(base, "src", tag)
    os.makedirs(d, exist_ok=True)
    segs = []
    for name, off in [("bootloader.bin", 0x1000), ("partitions.bin", 0x8000),
                      ("boot_app0.bin", 0xE000), ("firmware.bin", 0x10000)]:
        with open(os.path.join(d, name), "wb") as f:
            f.write(b"x" * 16)
        segs.append({"file": name, "offset": off, "size": 16, "sha256": "0" * 64})
    with open(os.path.join(d, "manifest.json"), "w") as f:
        json.dump({"version": ver, "chip": "esp32", "flash_size": "16MB", "segments": segs}, f, indent=2)
