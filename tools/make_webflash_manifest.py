#!/usr/bin/env python3
"""Web flasher support (task 102): ESP Web Tools manifests and releases.json.

  select --api releases.json [--cap 10]
      Print the tags to publish, newest first (one per line).
  build  --api releases.json --src DIR --out DIST/install [--cap 10]
      DIR/<tag>/ holds a release's manifest.json + segment files. For every selected tag that has
      a folder: copy the segment files to OUT/firmware/<tag>/, write the ESP Web Tools manifest
      (manifest.json there) and write OUT/releases.json.
  convert --in manifest.json --out webtools.json
      One manifest conversion only.

--api is the JSON list from `gh api "repos/OWNER/REPO/releases?per_page=100"`.
Rules: drafts and releases without a manifest.json asset are skipped; the newest CAP stable
releases are kept, plus any pre-release newer than the newest kept stable one.
Offsets come from our manifest.json, never hard-coded. No erase, so NVS survives an update.
"""
import argparse
import json
import os
import shutil
import sys

CAP = 10
CHIPS = {"esp32": "ESP32"}


def convert(manifest, name="Inkhaus"):
    """Our manifest.json (task 061) -> ESP Web Tools manifest. Paths are relative (same origin)."""
    chip = CHIPS.get(manifest.get("chip", ""))
    if not chip:
        raise ValueError("unsupported chip: %r" % manifest.get("chip"))
    segs = manifest.get("segments") or []
    if not segs:
        raise ValueError("no segments")
    parts = [{"path": s["file"], "offset": int(s["offset"])} for s in segs]
    return {
        "name": name,
        "version": manifest["version"],
        "new_install_prompt_erase": False,
        "builds": [{"chipFamily": chip, "parts": parts}],
    }


def _date(rel):
    return rel.get("published_at") or rel.get("created_at") or ""


def usable(rel):
    if rel.get("draft"):
        return False
    return any(a.get("name") == "manifest.json" for a in rel.get("assets", []))


def select(releases, cap=CAP):
    """Return the releases to publish, newest first."""
    rels = sorted((r for r in releases if usable(r)), key=_date, reverse=True)
    stable = [r for r in rels if not r.get("prerelease")][:cap]
    if not stable:
        return []
    newest = _date(stable[0])
    pre = [r for r in rels if r.get("prerelease") and _date(r) > newest]
    return sorted(pre + stable, key=_date, reverse=True)


def build(releases, src, out, cap=CAP):
    entries = []
    for rel in select(releases, cap):
        tag = rel["tag_name"]
        sdir = os.path.join(src, tag)
        mpath = os.path.join(sdir, "manifest.json")
        if not os.path.isfile(mpath):
            print("skip %s: no downloaded manifest.json" % tag, file=sys.stderr)
            continue
        with open(mpath) as f:
            m = json.load(f)
        ddir = os.path.join(out, "firmware", tag)
        os.makedirs(ddir, exist_ok=True)
        size = 0
        for s in m["segments"]:
            shutil.copyfile(os.path.join(sdir, s["file"]), os.path.join(ddir, s["file"]))
            size += os.path.getsize(os.path.join(ddir, s["file"]))
        with open(os.path.join(ddir, "manifest.json"), "w") as f:
            json.dump(convert(m), f, indent=2)
            f.write("\n")
        entries.append({
            "tag": tag,
            "version": m["version"],
            "date": _date(rel)[:10],
            "prerelease": bool(rel.get("prerelease")),
            "size": size,
            "manifest": "firmware/%s/manifest.json" % tag,
        })
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "releases.json"), "w") as f:
        json.dump(entries, f, indent=2)
        f.write("\n")
    return entries


def _load(path):
    with open(path) as f:
        return json.load(f)


def main(argv=None):
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("select")
    s.add_argument("--api", required=True)
    s.add_argument("--cap", type=int, default=CAP)
    b = sub.add_parser("build")
    b.add_argument("--api", required=True)
    b.add_argument("--src", required=True)
    b.add_argument("--out", required=True)
    b.add_argument("--cap", type=int, default=CAP)
    c = sub.add_parser("convert")
    c.add_argument("--in", dest="inp", required=True)
    c.add_argument("--out", required=True)
    a = ap.parse_args(argv)
    if a.cmd == "select":
        for r in select(_load(a.api), a.cap):
            print(r["tag_name"])
    elif a.cmd == "build":
        e = build(_load(a.api), a.src, a.out, a.cap)
        print("web flasher: %d releases" % len(e))
    else:
        with open(a.out, "w") as f:
            json.dump(convert(_load(a.inp)), f, indent=2)
            f.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
