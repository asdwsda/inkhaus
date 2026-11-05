#!/usr/bin/env python3
"""Build the website into site/dist: copy page, css, fonts, logo and the needed docs/ui/png images.
Optional env INKHAUS_VERSION (e.g. 1.2.3) fills the "Version" line; omitted if unset."""
import os, re, shutil, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import guide


site = Path(__file__).resolve().parent
root = site.parent
out = site / "dist"
if out.exists():
    shutil.rmtree(out)
(out / "img").mkdir(parents=True)

html = (site / "index.html").read_text()
imgs = sorted(set(re.findall(r'src="img/([^"]+)"', html)))
for name in imgs:
    src = root / ("docs/ui" if name.endswith(".gif") else "docs/ui/png") / name
    if not src.is_file():
        sys.exit(f"missing image: {src}")
    shutil.copy(src, out / "img" / name)

ver = os.environ.get("INKHAUS_VERSION", "").strip().lstrip("v")
line = f'<p class="muted">Version {ver}</p>' if re.fullmatch(r"[0-9][0-9A-Za-z.\-]*", ver) else ""
html = html.replace("<!--VERSION-->", line)
(out / "index.html").write_text(html)
for f in ("style.css", "logo-inkhaus.svg"):
    shutil.copy(site / f, out / f)
shutil.copytree(site / "fonts", out / "fonts")
# Task 102: /install/ page. The workflow adds esp-web-tools/, firmware/ and releases.json next to it.
(out / "install").mkdir()
for f in ("install.html", "install.js", "provision-client.js", "config-check.js", "config-editor.js"):
    shutil.copy(site / f, out / "install" / ("index.html" if f == "install.html" else f))
npages = guide.build(root, out, site)
print(f"built {out} ({len(imgs)} images, {npages} guide pages)")
