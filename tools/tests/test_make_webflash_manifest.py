#!/usr/bin/env python3
"""Host test of tools/make_webflash_manifest.py (task 102). Run: python3 tools/tests/test_make_webflash_manifest.py"""
import json
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import make_webflash_manifest as w

SAMPLE = os.path.join(os.path.dirname(HERE), "testdata", "webflash")


def rel(tag, date, pre=False, man=True, draft=False):
    return {"tag_name": tag, "published_at": date, "prerelease": pre, "draft": draft,
            "assets": [{"name": "manifest.json" if man else "firmware.bin"}]}


class T(unittest.TestCase):
    def test_convert(self):
        with open(os.path.join(SAMPLE, "src", "v1.3.1", "manifest.json")) as f:
            m = json.load(f)
        m["segments"][2]["offset"] = 0xF000  # offsets come from the input
        o = w.convert(m)
        self.assertIs(o["new_install_prompt_erase"], False)
        self.assertEqual(o["version"], "1.3.1")
        b = o["builds"][0]
        self.assertEqual(b["chipFamily"], "ESP32")
        self.assertEqual([(p["path"], p["offset"]) for p in b["parts"]],
                         [("bootloader.bin", 0x1000), ("partitions.bin", 0x8000),
                          ("boot_app0.bin", 0xF000), ("firmware.bin", 0x10000)])
        self.assertTrue(all("/" not in p["path"] for p in b["parts"]))

    def test_convert_rejects(self):
        with self.assertRaises(ValueError):
            w.convert({"chip": "esp32s3", "version": "1", "segments": [{"file": "a", "offset": 0}]})
        with self.assertRaises(ValueError):
            w.convert({"chip": "esp32", "version": "1", "segments": []})

    def test_skip_rules(self):
        r = [rel("a", "2026-01-02"), rel("b", "2026-01-03", man=False), rel("c", "2026-01-04", draft=True)]
        self.assertEqual([x["tag_name"] for x in w.select(r)], ["a"])

    def test_cap_and_prerelease(self):
        r = [rel("v%d" % i, "2026-02-%02d" % i) for i in range(1, 16)]
        r += [rel("pre-new", "2026-03-01", pre=True), rel("pre-old", "2026-02-05", pre=True)]
        tags = [x["tag_name"] for x in w.select(r, 10)]
        self.assertEqual(len(tags), 11)  # 10 stable + the newer pre-release
        self.assertEqual(tags[0], "pre-new")
        self.assertEqual(tags[1], "v15")
        self.assertEqual(tags[-1], "v6")
        self.assertNotIn("pre-old", tags)
        self.assertEqual(w.select([rel("p", "2026-01-01", pre=True)]), [])

    def test_build_sample(self):
        with open(os.path.join(SAMPLE, "api.json")) as f:
            api = json.load(f)
        with tempfile.TemporaryDirectory() as out:
            e = w.build(api, os.path.join(SAMPLE, "src"), out)
            self.assertEqual([x["tag"] for x in e], ["v1.4.0-rc1", "v1.3.1", "v1.3.0", "v1.2.1"])
            with open(os.path.join(out, "releases.json")) as f:
                self.assertEqual(json.load(f), e)
            self.assertTrue(e[0]["prerelease"])
            self.assertFalse(e[1]["prerelease"])
            self.assertEqual(e[1]["size"], 64)
            self.assertEqual(e[1]["date"], "2026-11-02")
            d = os.path.join(out, "firmware", "v1.3.1")
            self.assertEqual(sorted(os.listdir(d)),
                             ["boot_app0.bin", "bootloader.bin", "firmware.bin", "manifest.json", "partitions.bin"])
            with open(os.path.join(d, "manifest.json")) as f:
                self.assertEqual(json.load(f)["builds"][0]["parts"][3]["offset"], 0x10000)


if __name__ == "__main__":
    unittest.main()
