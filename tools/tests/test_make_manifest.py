#!/usr/bin/env python3
"""Host test of tools/make_manifest.py (task 061). Run: python3 tools/tests/test_make_manifest.py"""
import hashlib
import json
import os
import tempfile
import unittest

import sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
import make_manifest as mm

CSV = """# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x640000,
app1,     app,  ota_1,   0x650000,0x640000,
"""


class T(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp()
        self.b = os.path.join(self.d, "b")
        os.makedirs(self.b)
        for n, c in [("bootloader.bin", b"b" * 10), ("partitions.bin", b"p" * 20), ("firmware.bin", b"f" * 30)]:
            open(os.path.join(self.b, n), "wb").write(c)
        self.app0 = os.path.join(self.d, "boot_app0.bin")
        open(self.app0, "wb").write(b"a" * 5)
        self.csv = os.path.join(self.d, "p.csv")
        open(self.csv, "w").write(CSV)
        self.vh = os.path.join(self.d, "version.h")
        open(self.vh, "w").write('#pragma once\n#define FW_VERSION "2.3.4"\n')
        self.out = os.path.join(self.d, "out")

    def run_main(self, tag=None):
        a = ["--build-dir", self.b, "--boot-app0", self.app0, "--partitions-csv", self.csv,
             "--out-dir", self.out, "--version-h", self.vh]
        if tag:
            a += ["--tag", tag]
        return mm.main(a)

    def test_manifest(self):
        self.assertEqual(self.run_main("v2.3.4"), 0)
        m = json.load(open(os.path.join(self.out, "manifest.json")))
        self.assertEqual((m["version"], m["chip"], m["flash_size"]), ("2.3.4", "esp32", "16MB"))
        got = {s["file"]: s for s in m["segments"]}
        self.assertEqual([s["offset"] for s in m["segments"]], [0x1000, 0x8000, 0xE000, 0x10000])
        self.assertEqual(got["firmware.bin"]["size"], 30)
        self.assertEqual(got["firmware.bin"]["sha256"], hashlib.sha256(b"f" * 30).hexdigest())

    def test_tag_mismatch(self):
        self.assertEqual(self.run_main("v2.3.5"), 1)
        self.assertFalse(os.path.exists(os.path.join(self.out, "manifest.json")))


if __name__ == "__main__":
    unittest.main()
