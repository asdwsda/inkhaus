#!/usr/bin/env python3
"""Build the release assets and manifest.json (task 061).

Usage: tools/make_manifest.py --build-dir .pio/build/m5paper --boot-app0 PATH/boot_app0.bin \
           --partitions-csv PATH/default_16MB.csv --out-dir OUT [--tag v1.0.0]

Version is read from include/version.h (FW_VERSION). With --tag, the tag (minus the leading
'v') must equal it, else exit 1. Copies the four segment files to OUT and writes OUT/manifest.json.
Offsets: bootloader 0x1000 and partition table 0x8000 are fixed for the ESP32; boot_app0 goes to
the otadata partition's offset and firmware.bin to the first app partition (both from the csv).
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BOOTLOADER_OFFSET = 0x1000
PARTITIONS_OFFSET = 0x8000
FLASH_SIZE = "16MB"


def read_version(path):
    with open(path) as f:
        m = re.search(r'#define\s+FW_VERSION\s+"([^"]+)"', f.read())
    if not m:
        raise SystemExit("FW_VERSION not found in " + path)
    return m.group(1)


def csv_offsets(path):
    """Return {name: offset} for the partitions csv; a name starting with a type 'app' is also 'first_app'."""
    out = {}
    with open(path) as f:
        for line in f:
            line = line.split("#")[0].strip()
            if not line:
                continue
            cols = [c.strip() for c in line.split(",")]
            if len(cols) < 5:
                continue
            out[cols[0]] = int(cols[3], 0)
            if cols[1] == "app" and "first_app" not in out:
                out["first_app"] = int(cols[3], 0)
            if cols[2] == "ota" and "otadata" not in out:
                out["otadata"] = int(cols[3], 0)
    return out


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        h.update(f.read())
    return h.hexdigest()


def build(version, build_dir, boot_app0, parts_csv, out_dir):
    offs = csv_offsets(parts_csv)
    segs = [
        ("bootloader.bin", os.path.join(build_dir, "bootloader.bin"), BOOTLOADER_OFFSET),
        ("partitions.bin", os.path.join(build_dir, "partitions.bin"), PARTITIONS_OFFSET),
        ("boot_app0.bin", boot_app0, offs["otadata"]),
        ("firmware.bin", os.path.join(build_dir, "firmware.bin"), offs["first_app"]),
    ]
    os.makedirs(out_dir, exist_ok=True)
    entries = []
    for name, src, off in segs:
        dst = os.path.join(out_dir, name)
        shutil.copyfile(src, dst)
        entries.append({"file": name, "offset": off, "size": os.path.getsize(dst), "sha256": sha256(dst)})
    manifest = {"version": version, "chip": "esp32", "flash_size": FLASH_SIZE, "segments": entries}
    with open(os.path.join(out_dir, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")
    return manifest


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", required=True)
    ap.add_argument("--boot-app0", required=True)
    ap.add_argument("--partitions-csv", required=True)
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--version-h", default=os.path.join(ROOT, "include", "version.h"))
    ap.add_argument("--tag", help="release tag, e.g. v1.0.0; must match FW_VERSION")
    a = ap.parse_args(argv)
    version = read_version(a.version_h)
    if a.tag is not None and a.tag != "v" + version:
        print("tag %s does not match FW_VERSION %s (expected v%s)" % (a.tag, version, version), file=sys.stderr)
        return 1
    build(version, a.build_dir, a.boot_app0, a.partitions_csv, a.out_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
