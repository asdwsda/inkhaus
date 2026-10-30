#!/usr/bin/env python3
"""Check site/dist: every local href/src/url() resolves; print page weight (without flow.gif)."""
import re, sys
from pathlib import Path

out = Path(__file__).resolve().parent / "dist"
bad = 0
for page in [out / "index.html", out / "style.css"]:
    text = page.read_text()
    refs = re.findall(r'(?:href|src)="([^"]+)"', text) + re.findall(r"url\(([^)]+)\)", text)
    for r in refs:
        r = r.strip("'\"")
        if re.match(r"(https?:|mailto:|#|data:)", r):
            continue
        if not (page.parent / r.split("#")[0]).is_file():
            print(f"BROKEN {page.name}: {r}"); bad += 1
ids = set(re.findall(r'id="([^"]+)"', (out / "index.html").read_text()))
for a in re.findall(r'href="#([^"]+)"', (out / "index.html").read_text()):
    if a not in ids:
        print(f"BROKEN anchor #{a}"); bad += 1
total = sum(f.stat().st_size for f in out.rglob("*") if f.is_file())
print(f"page weight: {total/1024:.0f} KiB")
if total > 1024 * 1024:
    print("over 1 MB"); bad += 1
sys.exit(1 if bad else 0)
