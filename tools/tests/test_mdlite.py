#!/usr/bin/env python3
"""Tests for site/mdlite.py (task 110). Run: python3 tools/tests/test_mdlite.py"""
import sys, unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "site"))
import mdlite


class T(unittest.TestCase):
    def test_heading_anchor_and_duplicates(self):
        h, heads = mdlite.render("# A b\n\n## Same\n\n## Same\n")
        self.assertIn('id="a-b"', h)
        self.assertEqual([x[1] for x in heads], ["a-b", "same", "same-2"])

    def test_code_block_escaped_with_copy(self):
        h, _ = mdlite.render("```yaml\na: <b>\n```\n")
        self.assertIn("a: &lt;b&gt;", h)
        self.assertIn('class="copy"', h)

    def test_figure_alt_and_caption(self):
        h, _ = mdlite.render('![Alt text](img/x.png "Cap *it*")\n')
        self.assertIn('alt="Alt text"', h)
        self.assertIn("<figcaption", h)

    def test_table_inline_and_html_escape(self):
        h, _ = mdlite.render("| K | V |\n|---|---|\n| `a` | **b** <i> |\n")
        self.assertIn("<td><code>a</code></td>", h)
        self.assertIn("<strong>b</strong> &lt;i&gt;", h)

    def test_lists_and_links(self):
        h, _ = mdlite.render("- one [x](y.md#z)\n- two\n\n1. a\n2. b\n")
        self.assertIn('<ul><li>one <a href="y.md#z">x</a></li>', h)
        self.assertIn("<ol><li>a</li>", h)


if __name__ == "__main__":
    unittest.main()
