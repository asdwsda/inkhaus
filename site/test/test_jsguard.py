#!/usr/bin/env python3
"""The guard catches each forbidden pattern and ignores comments. Run: python3 site/test/test_jsguard.py"""
import sys, unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from jsguard import forbidden_in, strip_js_comments

BAD = [
    "fetch('x')", "new XMLHttpRequest()", "navigator.sendBeacon(u)", "new WebSocket(u)", "new Image()",
    "localStorage.x", "sessionStorage.x", "document.cookie", "console.log(1)", "location.href", "eval('1')",
    "a.src = 1", "import('x.js')", "import ('x.js')", "window['fetch'](u)", "self[\"sendBeacon\"](u)", "x['localStorage']",
    "x[`sessionStorage`]", "e.setAttribute('src', u)", "e.setAttribute(\"href\",u)", "document.createElement('img')",
    "document.createElement(\"script\")", "document.createElement('iframe')", "createElement('link')", "form.submit()",
    "new Worker(u)", "new Audio(u)", "window[k]", "globalThis.x", "importScripts('x')", "postMessage(1)",
]
GOOD = ["const a = 1; // fetch('x')", "/* new Worker(u) */ var a;", "var s = 'http://x'; var t = 2; // window[", "e.setAttribute('class','x')",
        "createElement('div')", "x.submitted = 1"]


class T(unittest.TestCase):
    def test_bad(self):
        for c in BAD:
            self.assertTrue(forbidden_in(strip_js_comments(c)), c)

    def test_good(self):
        for c in GOOD:
            self.assertFalse(forbidden_in(strip_js_comments(c)), c)

    def test_comment_markers_in_strings_do_not_hide_code(self):
        self.assertTrue(forbidden_in(strip_js_comments("var u = 'http://x'; fetch(u)")))
        self.assertTrue(forbidden_in(strip_js_comments('var u = "//"; new Worker(u)')))
        self.assertTrue(forbidden_in(strip_js_comments("/* c */ fetch(u) /* d */")))
        self.assertEqual(strip_js_comments("a // c\nb").split(), ["a", "b"])


if __name__ == "__main__":
    unittest.main()
