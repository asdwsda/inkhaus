#!/usr/bin/env python3
"""Check site/dist (tasks 065, 102, 110).
- every local href/src/url() resolves, and every #anchor exists in its target page (all HTML pages)
- page weight: the home page plus assets, the install page, and each guide page (own HTML + the files
  it references) must stay under 1 MiB; flow.gif is not counted
- guide: every <img> has non-empty alt text
- config keys: the guide's config reference vs lib/config (parser) and the example files.
  Errors: parser key missing from the reference or vice versa; secrets.yaml key mismatch.
  Warnings: reference keys missing from config.example.yaml (known gap while widget tasks add examples)."""
import re, shutil, subprocess, sys
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote

here = Path(__file__).resolve().parent
sys.path.insert(0, str(here))
import configkeys
from jsguard import forbidden_in, strip_js_comments

out = here / "dist"
bad = 0
CAP = 1024 * 1024
# Task 102: install/esp-web-tools, install/firmware and releases.json are added by the pages workflow.
WORKFLOW_ONLY = ("esp-web-tools/", "releases.json")
SKIP_DIRS = (out / "install" / "firmware", out / "install" / "esp-web-tools")
LEGACY_SECRETS_KEYS = {"connection"}  # accepted by the parser, not in the example on purpose


def fail(msg):
    global bad
    print(msg)
    bad += 1


class Page(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids, self.hrefs, self.imgs = set(), [], []

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if "id" in a:
            self.ids.add(a["id"])
        for k in ("href", "src"):
            if a.get(k):
                self.hrefs.append(a[k])
        if tag == "img":
            self.imgs.append(a)


def parse(path):
    p = Page()
    p.feed(path.read_text())
    return p


def local(page, ref):
    ref = ref.strip("'\"")
    if re.match(r"(https?:|mailto:|data:)", ref) or ref.startswith(WORKFLOW_ONLY):
        return None
    target = page.parent / unquote(ref.split("#")[0]) if not ref.startswith("#") else page
    if target.is_dir():
        target = target / "index.html"
    return target


pages = sorted(out.rglob("*.html"))
pages = [p for p in pages if not any(s in p.parents for s in SKIP_DIRS)]
parsed = {p: parse(p) for p in pages}
refs_of = {}
for page in pages + [out / "style.css"]:
    pp = parsed.get(page)
    refs = list(pp.hrefs) if pp else []
    refs += re.findall(r"url\(([^)]+)\)", page.read_text()) if page.suffix == ".css" else []
    files = set()
    for r in refs:
        t = local(page, r)
        if t is None:
            continue
        if not t.is_file():
            fail(f"BROKEN {page.relative_to(out)}: {r}")
            continue
        files.add(t)
        frag = r.split("#", 1)[1] if "#" in r else ""
        if frag and t.suffix == ".html":
            ids = parsed[t].ids if t in parsed else parse(t).ids
            if frag not in ids:
                fail(f"BROKEN anchor {page.relative_to(out)}: {r}")
    refs_of[page] = files
for page in pages:
    if "guide" in page.relative_to(out).parts:
        for img in parsed[page].imgs:
            decorative = img.get("alt") == "" and img.get("src", "").endswith("logo-inkhaus.svg")
            if not decorative and not (img.get("alt") or "").strip():
                fail(f"NO ALT {page.relative_to(out)}: {img.get('src')}")
for f2 in sorted((out / "guide/img/2x").glob("*.png")):
    if not (f2.parent.parent / f2.name).is_file():
        fail(f"2x picture without 1x sibling: {f2.name}")


def weight(page):
    """HTML plus the non-HTML files it references, plus the stylesheet's fonts."""
    files = {page} | {f for f in refs_of[page] if f.suffix != ".html"}
    for f in list(files):
        if f.suffix == ".css":
            files |= refs_of.get(f, set())
    return sum(f.stat().st_size for f in files if f.name != "flow.gif")


for page in pages:
    rel = page.relative_to(out)
    if rel.parts[0] == "guide" or str(rel) in ("index.html", "install/index.html"):
        w = weight(page)
        print(f"page weight {rel}: {w/1024:.0f} KiB")
        if w > CAP:
            fail(f"over 1 MiB: {rel}")
total = sum(f.stat().st_size for f in out.rglob("*") if f.is_file() and not any(s in f.parents for s in SKIP_DIRS))
print(f"site total (information only): {total/1024:.0f} KiB")

# Config editor security (task 119): secrets are write-only and stay in memory. The editor code may not
# talk to the network, store anything, log, or change the URL; the secrets form may not invite a password
# manager to save it. Comments are stripped (string-aware) before the scan.
EDITOR_JS = ("config-editor.js", "provision-client.js", "config-check.js")
for js in EDITOR_JS:
    f = out / "install" / js
    if not f.is_file():
        fail(f"EDITOR file missing in dist: {js}")
        continue
    for name in forbidden_in(strip_js_comments(f.read_text())):
        fail(f"EDITOR SECURITY {js}: forbidden use of {name}")
inst = (out / "install" / "index.html").read_text()
for need in ("This page does not store, send or log what you type here.", "Write-only: the device never sends secrets back"):
    if need not in inst:
        fail(f"EDITOR notice missing in install page: {need!r}")
sec = re.search(r'<form id="sf".*?</form>', inst, re.S)
if not sec:
    fail("EDITOR secrets form not found")
else:
    for tag in re.findall(r"<input[^>]*>", sec.group(0)):
        if 'type="radio"' in tag or 'type="checkbox"' in tag:
            continue
        if 'autocomplete="off"' not in tag and 'autocomplete="new-password"' not in tag:
            fail(f"EDITOR secrets input without autocomplete off: {tag[:60]}")
        if re.search(r"\sname=", tag) or "value=" in tag:
            fail(f"EDITOR secrets input with name/value attribute: {tag[:60]}")
    if re.search(r"<form[^>]*\s(action|method)=", sec.group(0)):
        fail("EDITOR secrets form has action/method")
if re.search(r"<(script|link)[^>]+(src|href)=\"https?://", inst) and "esp-web-tools" not in inst:
    fail("EDITOR external script on the install page")
# The guard's own test.
r = subprocess.run([sys.executable, str(here / "test" / "test_jsguard.py")], capture_output=True, text=True)
if r.returncode != 0:
    fail("GUARD TEST failed\n" + r.stderr[-800:])
else:
    print("guard test ok")
# The JS unit tests (no device needed). Skipped, with a note, when node is missing.
if shutil.which("node"):
    for t in sorted((here / "test").glob("*.test.js")):
        r = subprocess.run(["node", str(t)], capture_output=True, text=True)
        if r.returncode != 0:
            fail(f"JS TEST failed: {t.name}\n{r.stdout[-800:]}{r.stderr[-400:]}")
        else:
            print(f"js test ok: {t.name}")
else:
    print("note: node not found, JS tests skipped")

# Config-key lint (task 110).
ref_md = next((here.parent / "docs/guide").glob("*config-reference.md"), None)
if ref_md is None:
    fail("no config reference page in docs/guide")
else:
    ref = configkeys.reference_keys(ref_md)
    cfg_ref, sec_ref = ref.get("config.yaml", set()), ref.get("secrets.yaml", set())
    code = configkeys.code_keys()
    for k in sorted(code - cfg_ref - sec_ref):
        fail(f"CONFIG KEY read by lib/config but not in the reference: {k}")
    for k in sorted((cfg_ref | sec_ref) - code):
        fail(f"CONFIG KEY in the reference but not read by lib/config: {k}")
    ex = configkeys.yaml_keys("config.example.yaml", skip_groups=True)
    for k in sorted(ex - cfg_ref):
        fail(f"CONFIG KEY in config.example.yaml but not in the reference: {k}")
    for k in sorted(cfg_ref - ex):
        print(f"warning: config key `{k}` is in the reference but not in config.example.yaml")
    sec_ex = configkeys.yaml_keys("secrets.example.yaml")
    for k in sorted(sec_ex - sec_ref):
        fail(f"CONFIG KEY in secrets.example.yaml but not in the reference: {k}")
    for k in sorted(sec_ref - sec_ex - LEGACY_SECRETS_KEYS):
        fail(f"CONFIG KEY in the reference (secrets.yaml) but not in secrets.example.yaml: {k}")
    # Cross-check the editor's key table (site/config-check.js KEYS) with the parser and the reference.
    if shutil.which("node"):
        r = subprocess.run(["node", "-e", "console.log(JSON.stringify(require(process.argv[1]).KEYS))", str(here / "config-check.js")],
                           capture_output=True, text=True)
        if r.returncode != 0:
            fail("could not load KEYS from config-check.js: " + r.stderr[-300:])
        else:
            import json
            js_keys = {k for v in json.loads(r.stdout).values() for k in v}
            for k in sorted(js_keys - code):
                fail(f"EDITOR KEY in config-check.js KEYS but not read by lib/config: {k}")
            for k in sorted(cfg_ref - js_keys):
                fail(f"EDITOR KEY in the reference but missing from config-check.js KEYS: {k}")
    else:
        print("note: node not found, KEYS cross-check skipped")
sys.exit(1 if bad else 0)
