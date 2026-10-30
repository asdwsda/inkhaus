#!/usr/bin/env python3
"""Check relative links and image paths in README.md and docs/*.md."""
import re, sys, pathlib
root = pathlib.Path(__file__).resolve().parent.parent
bad = 0
for f in [root / "README.md", *sorted((root / "docs").glob("*.md"))]:
    text = f.read_text()
    if re.search(r"TODO|TBD|FIXME", text):
        print(f"{f}: contains TODO/TBD"); bad += 1
    for m in re.finditer(r"!?\[[^\]]*\]\(([^)\s]+)\)", text):
        t = m.group(1).split("#")[0]
        if not t or re.match(r"[a-z]+:", t):
            continue
        if not (f.parent / t).exists():
            print(f"{f.relative_to(root)}: broken {m.group(1)}"); bad += 1
print("links ok" if not bad else f"{bad} problems")
sys.exit(1 if bad else 0)
