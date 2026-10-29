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
# Extra sizes for a few icons only (sleep indicator, STYLE.md 5.5): 28 and 32 px.
EXTRA_SIZES = {"i-moon": (28, 32), "i-sidebtn": (32,), "i-finger": (32,)}

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

# Binary sensor icon pairs (widgets.md 11.1): <name>_on solid / filled parts, <name>_off outline.
_S = 'fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"'
_F = 'fill="currentColor" stroke="currentColor" stroke-width="1" stroke-linejoin="round"'
_BANG_INK = ('<path d="M12 9.5 V14" stroke="currentColor" stroke-width="2" stroke-linecap="round"/>'
             '<circle cx="12" cy="17" r="1.2" fill="currentColor"/>')
_BANG_PAPER = ('<path d="M12 9.5 V14" stroke="#fff" stroke-width="2" stroke-linecap="round"/>'
               '<circle cx="12" cy="17" r="1.2" fill="#fff"/>')
_FIG = "M13 8 L11 14 L14 17 L13 22 M11 14 L8 21 M13 8 L17 11 M13 8 L9 10"
_CLOUD = "M7 13 A3.5 3.5 0 0 1 7.5 6.1 A5 5 0 0 1 17 7 A3.2 3.2 0 0 1 17 13 Z"
_BOLT = "M13 2 L5 13.5 H11 L10 22 L19 10 H13 Z"
EXTRA.update({
    "i-door-on": f'<path d="M4 3 H20 V21 H4 Z" {_S}/><path d="M4 3 L12 5.5 V21 H4 Z" fill="currentColor"/>',
    "i-door-off": f'<path d="M4 3 H20 V21 H4 Z" {_S}/><path d="M8 6.5 H16 V21 H8 Z" {_S}/>'
                  '<circle cx="14" cy="14" r="1.1" fill="currentColor"/>',
    "i-garage-on": f'<path d="M2 10 L12 3 L22 10 V21 H2 Z" {_S}/><rect x="6" y="13" width="12" height="8" fill="currentColor"/>',
    "i-garage-off": f'<path d="M2 10 L12 3 L22 10 V21 H2 Z" {_S}/><path d="M6 12 H18 V21 H6 Z M6 15 H18 M6 18 H18" '
                    'fill="none" stroke="currentColor" stroke-width="1.6"/>',
    "i-motion-on": f'<circle cx="14" cy="4.5" r="2.4" fill="currentColor"/><path d="{_FIG}" {_S}/>'
                   '<path d="M6 8 Q4 12 6 16 M3 6 Q0.8 12 3 18" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round"/>',
    "i-motion-off": f'<circle cx="14" cy="4.5" r="2" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="{_FIG}" {_S}/>',
    "i-person-on": '<circle cx="12" cy="7" r="4" fill="currentColor"/><path d="M4 21 A8 7 0 0 1 20 21 Z" fill="currentColor"/>',
    "i-person-off": f'<circle cx="12" cy="7" r="3.6" {_S}/><path d="M4.5 21 A7.5 6.5 0 0 1 19.5 21 Z" {_S}/>',
    "i-puddle-on": f'<path d="M12 2 C12 2 6.5 8 6.5 12 a5.5 5.5 0 0 0 11 0 C17.5 8 12 2 12 2z" {_F}/>'
                   f'<path d="M2 21 q2.5 -2.5 5 0 t5 0 t5 0 t5 0" {_S}/>',
    "i-puddle-off": f'<path d="M12 2 C12 2 6.5 8 6.5 12 a5.5 5.5 0 0 0 11 0 C17.5 8 12 2 12 2z" {_S}/>'
                    f'<path d="M2 21 q2.5 -2.5 5 0 t5 0 t5 0 t5 0" {_S}/>',
    "i-smoke-on": f'<path d="{_CLOUD}" {_F}/><path d="M8 16 q-1.5 2 0 4 M12 16 q-1.5 2 0 4 M16 16 q-1.5 2 0 4" {_S}/>',
    "i-smoke-off": f'<path d="{_CLOUD}" {_S}/><path d="M8 16 q-1.5 2 0 4 M12 16 q-1.5 2 0 4 M16 16 q-1.5 2 0 4" {_S}/>',
    "i-gas-on": f'<rect x="5" y="9" width="9" height="13" rx="3" fill="currentColor"/><path d="M9.5 9 V4.5 M7.5 4.5 H11.5" {_S}/>'
                '<circle cx="17.5" cy="8" r="2.2" fill="currentColor"/><circle cx="20" cy="12" r="1.6" fill="currentColor"/>',
    "i-gas-off": f'<rect x="5" y="9" width="9" height="13" rx="3" {_S}/><path d="M9.5 9 V4.5 M7.5 4.5 H11.5" {_S}/>'
                 '<circle cx="17.5" cy="8" r="2" fill="none" stroke="currentColor" stroke-width="1.6"/>'
                 '<circle cx="20" cy="12.5" r="1.4" fill="none" stroke="currentColor" stroke-width="1.4"/>',
    "i-problem-on": f'<path d="M12 3 L22 20 H2 Z" {_F}/>{_BANG_PAPER}',
    "i-problem-off": f'<path d="M12 3 L22 20 H2 Z" {_S}/>{_BANG_INK}',
    "i-battery-low": '<rect x="1" y="6" width="19" height="12" fill="none" stroke="currentColor" stroke-width="2"/>'
                     '<rect x="21" y="9.5" width="2" height="5" fill="currentColor"/>'
                     '<path d="M10.5 8.8 V13" stroke="currentColor" stroke-width="2" stroke-linecap="round"/>'
                     '<circle cx="10.5" cy="15.4" r="1" fill="currentColor"/>',
    "i-lock-open": f'<rect x="5" y="11" width="14" height="10" rx="1.5" {_S}/><path d="M8 11 V7 A4 4 0 0 1 16 7" {_S}/>',
    "i-lock-closed": f'<rect x="5" y="11" width="14" height="10" rx="1.5" fill="currentColor"/>'
                     f'<path d="M8 11 V7 A4 4 0 0 1 16 7 V11" {_S}/>',
    "i-plug-off": '<g fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round">'
                  '<rect x="8" y="2" width="2.5" height="6"/><rect x="13.5" y="2" width="2.5" height="6"/>'
                  '<path d="M6 8 H18 V12 A6 6 0 0 1 6 12 Z"/><rect x="10.75" y="17.5" width="2.5" height="4.5"/></g>',
    "i-bolt": f'<path d="{_BOLT}" {_F}/>',
    "i-bolt-off": f'<path d="{_BOLT}" {_S}/>',
})


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


LOGO_SVG = ROOT / "docs/ui/logo-inkling.svg"
LOGO_SIZES = (360, 48)  # splash (docs/ui/splash.md 6) and the 48 px icon


def render_logo(size, tmp):
    """Alpha bitmap (255 = ink) of docs/ui/logo-inkling.svg at size x size on white."""
    png = Path(tmp) / f"logo{size}.png"
    subprocess.run(["inkscape", str(LOGO_SVG), "--export-type=png", f"--export-width={size}",
                    f"--export-height={size}", "--export-background=white", "--export-background-opacity=1",
                    f"--export-filename={png}"], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    img = Image.open(png).convert("L")
    assert img.size == (size, size), img.size
    return [255 - v for v in img.tobytes()]


def main():
    syms = collect_symbols()
    names = sorted(syms)
    with tempfile.TemporaryDirectory() as tmp:
        data = {s: render(syms, names, s, tmp) for s in SIZES}
        for sz in sorted({z for v in EXTRA_SIZES.values() for z in v}):
            data[sz] = render(syms, [n for n in names if sz in EXTRA_SIZES.get(n, ())], sz, tmp)
        logos = {z: render_logo(z, tmp) for z in LOGO_SIZES}

    hdr = ["// Generated by tools/gen_icons.py from the mockup symbols (docs/ui). Do not edit.",
           "#pragma once", "#include <stdint.h>", "", "namespace render {", "",
           "enum class Icon : uint8_t {"]
    hdr += [f"  {camel(n)}," for n in names]
    hdr += ["  Count", "};", "", "// 8-bit coverage bitmap (0 = nothing, 255 = ink), size x size, row-major.",
            "// Sizes: 24 (S), 40 (M), 48 (L); moon also 28 and 32, sidebtn and finger also 32. nullptr for other sizes.",
            "const uint8_t* iconBitmap(Icon icon, int size);", "",
            "// The Inkling logo (docs/ui/logo-inkling.svg), same format, sizes 360 (splash) and 48. nullptr for others.",
            "const uint8_t* logoBitmap(int size);", "", "}  // namespace render", ""]
    (ROOT / "src/render/icon_data.h").write_text("\n".join(hdr))

    src = ["// Generated by tools/gen_icons.py from the mockup symbols (docs/ui). Do not edit.",
           '#include "icon_data.h"', "", "namespace render {", "namespace {"]
    for s in sorted(data):
        for n in names:
            if s not in SIZES and s not in EXTRA_SIZES.get(n, ()):
                continue
            vals = data[s][n]
            body = ",".join(str(v) for v in vals)
            src.append(f"const uint8_t k{camel(n)}{s}[{s * s}] = {{{body}}};")
    for z in LOGO_SIZES:
        src.append(f"const uint8_t kLogo{z}[{z * z}] = {{{','.join(str(v) for v in logos[z])}}};")
    src.append("}  // namespace")
    src.append("")
    src.append("const uint8_t* logoBitmap(int size) {")
    src.append("  return " + " : ".join(f"size == {z} ? kLogo{z}" for z in LOGO_SIZES) + " : nullptr;")
    src.append("}")
    src.append("")
    src.append("const uint8_t* iconBitmap(Icon icon, int size) {")
    src.append("  switch (icon) {")
    for n in names:
        c = camel(n)
        expr = "nullptr"
        for z in reversed(sorted(list(SIZES) + list(EXTRA_SIZES.get(n, ())))):
            expr = f"size == {z} ? k{c}{z} : {expr}"
        src.append(f"    case Icon::{c}: return {expr};")
    src.append("    default: return nullptr;")
    src.append("  }")
    src.append("}")
    src.append("")
    src.append("}  // namespace render")
    (ROOT / "src/render/icon_data.cpp").write_text("\n".join(src) + "\n")
    print(f"{len(names)} icons: {' '.join(names)}")


if __name__ == "__main__":
    sys.exit(main())
