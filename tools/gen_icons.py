#!/usr/bin/env python3
"""Generate src/render/icon_data.{h,cpp}: 8-bit alpha bitmaps of the UI icons at 24/40/48 px.

The icons are the <symbol>s of the approved mockups (docs/ui/*.svg, docs/ui/flow/*.svg), rendered with
Inkscape (the renderer that made docs/ui/png/*), so firmware icons match the PNGs. A few icons the
mockups do not contain (snow, fan, dry, power, wifi_dot, wifi_off, clock, sunny, weather variants,
dot_on/dot_off, unknown) are defined below as simple designs of this tool.

Usage: tools/gen_icons.py   (needs inkscape and Pillow)
"""
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SIZES = (24, 40, 48)

EXTRA = {
    "i-snow": '<g fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round">'
              '<path d="M12 2.5 V21.5 M3.8 7.25 L20.2 16.75 M3.8 16.75 L20.2 7.25"/>'
              '<path d="M9 4.5 L12 7.5 L15 4.5 M9 19.5 L12 16.5 L15 19.5"/></g>',
    "i-power": '<g fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round">'
               '<path d="M12 3 V12"/><path d="M6.8 6.6 A8.4 8.4 0 1 0 17.2 6.6"/></g>',
    "i-fan": '<g fill="currentColor"><circle cx="12" cy="12" r="2"/>'
             '<ellipse cx="12" cy="6" rx="2.8" ry="4"/><ellipse cx="12" cy="18" rx="2.8" ry="4"/>'
             '<ellipse cx="6" cy="12" rx="4" ry="2.8"/><ellipse cx="18" cy="12" rx="4" ry="2.8"/></g>',
    "i-dry": '<path d="M12 2 C12 2 5 10 5 15 a7 7 0 0 0 14 0 C19 10 12 2 12 2z" fill="none" '
             'stroke="currentColor" stroke-width="2"/><path d="M4 20 L20 4" stroke="currentColor" '
             'stroke-width="2.4" stroke-linecap="round"/>',
    "i-wifi-dot": '<circle cx="12" cy="19.5" r="2.6" fill="currentColor"/>',
    "i-wifi-off": '<g fill="none" stroke="currentColor" stroke-width="2"><path d="M2 9 A14 14 0 0 1 22 9"/>'
                  '<path d="M5.5 12.5 A9.5 9.5 0 0 1 18.5 12.5"/><path d="M9 16 A5 5 0 0 1 15 16"/></g>'
                  '<circle cx="12" cy="19.5" r="1.8" fill="currentColor"/>'
                  '<path d="M3 3 L21 21" stroke="currentColor" stroke-width="2.4" stroke-linecap="round"/>',
    "i-clock": '<g fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round">'
               '<circle cx="12" cy="12" r="9"/><path d="M12 6.5 V12 L16 14.5"/></g>',
    "i-dot-on": '<circle cx="12" cy="12" r="8" fill="currentColor"/>',
    "i-dot-off": '<circle cx="12" cy="12" r="7" fill="none" stroke="currentColor" stroke-width="2"/>',
    "i-sunny": '<circle cx="12" cy="12" r="5" fill="currentColor"/><g stroke="currentColor" stroke-width="2" '
               'stroke-linecap="round"><path d="M12 2 V4.5 M12 19.5 V22 M2 12 H4.5 M19.5 12 H22 '
               'M4.9 4.9 L6.7 6.7 M17.3 17.3 L19.1 19.1 M4.9 19.1 L6.7 17.3 M17.3 6.7 L19.1 4.9"/></g>',
    "i-partlycloudy": '<circle cx="8" cy="8" r="3.4" fill="currentColor"/><g stroke="currentColor" '
                      'stroke-width="1.8" stroke-linecap="round"><path d="M8 1.5 V2.8 M1.5 8 H2.8 '
                      'M3.4 3.4 L4.3 4.3 M12.6 3.4 L11.7 4.3"/></g>'
                      '<path d="M9 21 A4 4 0 0 1 8.6 13.1 A5.4 5.4 0 0 1 19 12.3 A4.4 4.4 0 0 1 18.4 21 Z" '
                      'fill="currentColor" stroke="#fff" stroke-width="1.6"/>',
    "i-rainy": '<path d="M7 15 A4.5 4.5 0 0 1 6.6 6.05 A6 6 0 0 1 18.2 5.2 A5 5 0 0 1 17.5 15 Z" '
               'fill="currentColor"/><g stroke="currentColor" stroke-width="2" stroke-linecap="round">'
               '<path d="M8 18 L7 21 M12.5 18 L11.5 21 M17 18 L16 21"/></g>',
    "i-snowy": '<path d="M7 15 A4.5 4.5 0 0 1 6.6 6.05 A6 6 0 0 1 18.2 5.2 A5 5 0 0 1 17.5 15 Z" '
               'fill="currentColor"/><g fill="currentColor"><circle cx="8" cy="19.5" r="1.6"/>'
               '<circle cx="12.5" cy="19.5" r="1.6"/><circle cx="17" cy="19.5" r="1.6"/></g>',
    "i-lightning": '<path d="M7 15 A4.5 4.5 0 0 1 6.6 6.05 A6 6 0 0 1 18.2 5.2 A5 5 0 0 1 17.5 15 Z" '
                   'fill="currentColor"/><polygon points="13,14 8.5,20 12,20 10.5,23.5 16,17.5 12.5,17.5" '
                   'fill="currentColor" stroke="#fff" stroke-width="0.8"/>',
    "i-fog": '<g fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round">'
             '<path d="M4 7 H20 M4 12 H20 M4 17 H20"/></g>',
    "i-unknown": '<circle cx="12" cy="12" r="9" fill="none" stroke="currentColor" stroke-width="2"/>'
                 '<path d="M9.2 9.6 A2.8 2.8 0 1 1 12.8 12.3 C12.2 12.7 12 13.1 12 14" fill="none" '
                 'stroke="currentColor" stroke-width="2" stroke-linecap="round"/>'
                 '<circle cx="12" cy="17.4" r="1.3" fill="currentColor"/>',
}


def collect_symbols():
    syms = {}
    files = sorted((ROOT / "docs/ui").glob("*.svg")) + sorted((ROOT / "docs/ui/flow").glob("*.svg"))
    pat = re.compile(r'<symbol id="(i-[^"]+)" viewBox="0 0 24 24">(.*?)</symbol>', re.S)
    for f in files:
        for m in pat.finditer(f.read_text()):
            syms.setdefault(m.group(1), m.group(2))
    for k, v in EXTRA.items():
        syms.setdefault(k, v)
    return syms


def camel(sym_id):
    return "".join(p.capitalize() for p in sym_id[2:].split("-"))


def render(syms, names, size, tmp):
    cols = 8
    pitch = size + 8
    rows = (len(names) + cols - 1) // cols
    w, h = cols * pitch, rows * pitch
    out = ['<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
           f'width="{w}" height="{h}" viewBox="0 0 {w} {h}"><defs>']
    for n in names:
        out.append(f'<symbol id="{n}" viewBox="0 0 24 24">{syms[n]}</symbol>')
    out.append(f'</defs><rect width="{w}" height="{h}" fill="#fff"/>')
    for i, n in enumerate(names):
        x, y = (i % cols) * pitch + 4, (i // cols) * pitch + 4
        out.append(f'<use xlink:href="#{n}" href="#{n}" x="{x}" y="{y}" width="{size}" height="{size}" color="#000"/>')
    out.append("</svg>")
    svg = Path(tmp) / f"sheet{size}.svg"
    png = Path(tmp) / f"sheet{size}.png"
    svg.write_text("".join(out))
    subprocess.run(["inkscape", str(svg), "--export-type=png", "--export-dpi=96",
                    "--export-background=white", "--export-background-opacity=1",
                    f"--export-filename={png}"], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    img = Image.open(png).convert("L")
    assert img.size == (w, h), (img.size, (w, h))
    res = {}
    for i, n in enumerate(names):
        x, y = (i % cols) * pitch + 4, (i // cols) * pitch + 4
        tile = img.crop((x, y, x + size, y + size))
        res[n] = [255 - v for v in tile.tobytes()]
    return res


def main():
    syms = collect_symbols()
    names = sorted(syms)
    with tempfile.TemporaryDirectory() as tmp:
        data = {s: render(syms, names, s, tmp) for s in SIZES}

    hdr = ["// Generated by tools/gen_icons.py from the mockup symbols (docs/ui). Do not edit.",
           "#pragma once", "#include <stdint.h>", "", "namespace render {", "",
           "enum class Icon : uint8_t {"]
    hdr += [f"  {camel(n)}," for n in names]
    hdr += ["  Count", "};", "", "// 8-bit coverage bitmap (0 = nothing, 255 = ink), size x size, row-major.",
            "// Sizes: 24 (S), 40 (M), 48 (L). nullptr for other sizes.",
            "const uint8_t* iconBitmap(Icon icon, int size);", "", "}  // namespace render", ""]
    (ROOT / "src/render/icon_data.h").write_text("\n".join(hdr))

    src = ["// Generated by tools/gen_icons.py from the mockup symbols (docs/ui). Do not edit.",
           '#include "icon_data.h"', "", "namespace render {", "namespace {"]
    for s in SIZES:
        for n in names:
            vals = data[s][n]
            body = ",".join(str(v) for v in vals)
            src.append(f"const uint8_t k{camel(n)}{s}[{s * s}] = {{{body}}};")
    src.append("}  // namespace")
    src.append("")
    src.append("const uint8_t* iconBitmap(Icon icon, int size) {")
    src.append("  switch (icon) {")
    for n in names:
        c = camel(n)
        src.append(f"    case Icon::{c}: return size == 24 ? k{c}24 : size == 40 ? k{c}40 : size == 48 ? k{c}48 : nullptr;")
    src.append("    default: return nullptr;")
    src.append("  }")
    src.append("}")
    src.append("")
    src.append("}  // namespace render")
    (ROOT / "src/render/icon_data.cpp").write_text("\n".join(src) + "\n")
    print(f"{len(names)} icons: {' '.join(names)}")


if __name__ == "__main__":
    sys.exit(main())
