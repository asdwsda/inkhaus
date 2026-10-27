#!/usr/bin/env python3
"""Check UI mockups for overflow and overlap, using Inkscape's real text bounding boxes.

Convention (docs/ui/STYLE.md): each widget is a <g id="w-N"> containing a frame
<rect id="w-N-frame"> and text/icon elements with ids starting "w-N-". Status/tab bar
items use the same pattern with ids "bar-...". Every "w-N-*" element must lie inside
the frame shrunk by the padding, and no two text/icon elements of one widget may overlap.

Usage: tools/check_mockups.py [--pad 12] file.svg [...]
Exit code 1 if any problem is found.
"""
import argparse
import re
import subprocess
import sys


def query(svg):
    out = subprocess.run(["inkscape", "--query-all", svg], capture_output=True, text=True, check=True).stdout
    boxes = {}
    for line in out.splitlines():
        parts = line.split(",")
        if len(parts) == 5:
            boxes[parts[0]] = tuple(float(v) for v in parts[1:])
    return boxes


def check(svg, pad):
    boxes = query(svg)
    problems = []
    frames = {m.group(1): b for k, b in boxes.items() if (m := re.fullmatch(r"(w-\d+)-frame", k))}
    if not frames:
        problems.append(f"{svg}: no widget frames found (ids 'w-N-frame')")
    for wid, (fx, fy, fw, fh) in sorted(frames.items()):
        inner = (fx + pad, fy + pad, fx + fw - pad, fy + fh - pad)
        items = {k: b for k, b in boxes.items() if k.startswith(wid + "-") and k != wid + "-frame"
                 and not k.endswith("-hit") and not k.endswith("-btn")}
        for k, (x, y, w, h) in items.items():
            tol = 0.5
            if x < inner[0] - tol or y < inner[1] - tol or x + w > inner[2] + tol or y + h > inner[3] + tol:
                problems.append(f"{svg}: {k} bbox ({x:.1f},{y:.1f},{w:.1f},{h:.1f}) outside padded frame of {wid} "
                                f"({inner[0]:.0f},{inner[1]:.0f})-({inner[2]:.0f},{inner[3]:.0f})")
        keys = sorted(items)
        for i, a in enumerate(keys):
            for b in keys[i + 1:]:
                ax, ay, aw, ah = items[a]
                bx, by, bw, bh = items[b]
                if a.startswith(b) or b.startswith(a):
                    continue
                if ax < bx + bw and bx < ax + aw and ay < by + bh and by < ay + ah:
                    problems.append(f"{svg}: {a} overlaps {b}")
    return problems


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pad", type=float, default=12)
    ap.add_argument("svgs", nargs="+")
    args = ap.parse_args()
    problems = [p for svg in args.svgs for p in check(svg, args.pad)]
    for p in problems:
        print(p)
    print(f"{len(problems)} problem(s)")
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
