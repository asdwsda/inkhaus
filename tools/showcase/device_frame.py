#!/usr/bin/env python3
"""Real M5Paper frame for the showcase pictures (task 121). Numbers and sources: docs/ui/showcase-device.md.
Module: frame(orient, variant, x0, y0, s, screen) returns (svg, info) for one device; used by
gen_showcase_composites.py and gen_showcase_tour.py. Run as a script: writes the four spec mockups
docs/ui/showcase-device-{clean,photo}-{landscape,portrait}.svg (screen = showcase-tab-*.svg at 1:1).
variant "clean" = black outline, white fills (guide, GIF); "photo" = shaded (landing page hero only)."""
import os, re
UI = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "docs", "ui") + os.sep
K = 9.259  # px per mm at scale 1 (screen 58.32 mm = 540 px)
CW, CH = 67.0, 118.6
SCR_W, SCR_H = 58.32, 103.68
BZ_L = (CW - SCR_W) / 2          # 4.34
BZ_T = 4.3
ROCK_C, ROCK_L = CH - 60.0, 12.5   # v of the rocker centre (from the top), length
SD = (CH - 35.8, CH - 24.7)        # microSD, right edge
GB = (CH - 23.4, CH - 11.4)        # PORT.B, left edge
GC = (CH - 37.4, CH - 25.4)        # PORT.C
USB = (6.2, 15.3)
PA = (CW - 4.5 - 12, CW - 4.5)

DEFS_PHOTO = '''<linearGradient id="g-case" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#ffffff"/><stop offset="0.55" stop-color="#ececec"/><stop offset="1" stop-color="#cdcdcd"/></linearGradient>
<linearGradient id="g-rock" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#9a9a9a"/><stop offset="0.45" stop-color="#e8e8e8"/><stop offset="1" stop-color="#6a6a6a"/></linearGradient>
<radialGradient id="g-bg" cx="0.45" cy="0.4" r="0.8"><stop offset="0" stop-color="#f1f1f1"/><stop offset="1" stop-color="#c8c8c8"/></radialGradient>
<linearGradient id="g-sheen" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#fff" stop-opacity="0.16"/><stop offset="0.4" stop-color="#fff" stop-opacity="0"/><stop offset="1" stop-color="#fff" stop-opacity="0"/></linearGradient>'''


def shadow_filter(s=1.0):
    return f'<filter id="f-shadow" x="-10%" y="-10%" width="125%" height="125%"><feGaussianBlur stdDeviation="{14 * s:g}"/></filter>'


def inner(name):
    t = open(UI + name).read()
    t = re.sub(r"^.*?<svg[^>]*>", "", t, count=1, flags=re.S)
    return t.rsplit("</svg>", 1)[0]


def body(variant, sw=0.35):
    """Device in portrait mm coordinates (u right, v down)."""
    p = []
    a = p.append
    real = variant == "photo"
    if real:
        a(f'<rect id="dev-slab" x="0.9" y="0.9" width="{CW}" height="{CH}" rx="3" fill="#b4b4b4"/>')
        a(f'<rect id="dev-case" x="0" y="0" width="{CW}" height="{CH}" rx="3" fill="url(#g-case)" stroke="#8c8c8c" stroke-width="0.25"/>')
        a(f'<rect x="0.35" y="0.35" width="{CW-0.7}" height="{CH-0.7}" rx="2.7" fill="none" stroke="#fff" stroke-width="0.4" opacity="0.9"/>')
    else:
        a(f'<rect id="dev-case" x="0" y="0" width="{CW}" height="{CH}" rx="3" fill="#fff" stroke="#000" stroke-width="{sw}"/>')
    gx, gy = BZ_L - 0.9, BZ_T - 0.9
    gw, gh = SCR_W + 1.8, SCR_H + 1.8
    if real:
        a(f'<rect id="dev-glass" x="{gx}" y="{gy}" width="{gw}" height="{gh}" rx="1" fill="#cfcfcf" stroke="#9a9a9a" stroke-width="0.25"/>')
        a(f'<rect x="{BZ_L-0.4}" y="{BZ_T-0.4}" width="{SCR_W+0.8}" height="{SCR_H+0.8}" fill="none" stroke="#6a6a6a" stroke-width="0.3"/>')
    else:
        a(f'<rect id="dev-glass" x="{gx}" y="{gy}" width="{gw}" height="{gh}" rx="1" fill="#fff" stroke="#888" stroke-width="0.25"/>')

    def notch(x, y, w, h, r=0.5, fill="#000"):
        a(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" rx="{r}" fill="{fill}"/>')
    notch(USB[0], CH - 1.2, USB[1] - USB[0], 1.2, 0.6)
    notch(PA[0], CH - 1.4, PA[1] - PA[0], 1.4, 0.3)
    notch(0, GB[0], 1.4, GB[1] - GB[0], 0.3)
    notch(0, GC[0], 1.4, GC[1] - GC[0], 0.3)
    a(f'<rect x="{CW-1.0}" y="{SD[0]:.2f}" width="1.0" height="{SD[1]-SD[0]:.2f}" rx="0.2" fill="#555" stroke="#000" stroke-width="0.2"/>')
    top = ROCK_C - ROCK_L / 2
    if real:
        a(f'<rect id="dev-rocker" x="{CW-1.6}" y="{top}" width="2.6" height="{ROCK_L}" rx="0.9" fill="url(#g-rock)" stroke="#555" stroke-width="0.25"/>')
        a(f'<rect x="{CW-0.5}" y="{top+0.8}" width="0.5" height="{ROCK_L-1.6}" rx="0.25" fill="#fff" opacity="0.8"/>')
    else:
        a(f'<rect id="dev-rocker" x="{CW-1.6}" y="{top}" width="2.6" height="{ROCK_L}" rx="0.9" fill="#000" stroke="#000" stroke-width="{sw}"/>')
        for d in (-2.1, 2.1):
            a(f'<rect x="{CW-1.6}" y="{ROCK_C+d-0.1}" width="2.6" height="0.2" fill="#fff"/>')
    return "\n".join(a_ for a_ in p)


def size(orient, s=1.0):
    k = K * s
    return (CH * k, CW * k) if orient == "landscape" else (CW * k, CH * k)


def frame(orient, variant, x0, y0, s, screen, rnd=True):
    """One device with its case at top-left (x0, y0), scale s (s=1: screen pixels 1:1). `screen` is SVG drawn in
    screen pixels (540x960 or 960x540); it is scaled by s. Returns (svg, (sx, sy)) with the screen origin."""
    land = orient == "landscape"
    k = K * s
    if land:
        tf = f"matrix(0,{-k:.4f},{k:.4f},0,{x0:.2f},{y0 + CW*k:.2f})"
        sx, sy = x0 + BZ_T * k, y0 + (CW - BZ_L - SCR_W) * k
    else:
        tf = f"translate({x0:.2f},{y0:.2f}) scale({k:.4f})"
        sx, sy = x0 + BZ_L * k, y0 + BZ_T * k
    if rnd:
        sx, sy = round(sx), round(sy)
    real = variant == "photo"
    o = []
    if real:
        o.append(f'<g filter="url(#f-shadow)" opacity="0.55"><g transform="translate({22*s:g},{26*s:g})"><g transform="{tf}"><rect x="0" y="0" width="{CW}" height="{CH}" rx="3" fill="#000"/></g></g></g>')
    o.append(f'<g id="dev-body" transform="{tf}">\n{body(variant)}\n</g>')
    sw, sh = (960, 540) if land else (540, 960)
    o.append(f'<g id="dev-screen" transform="translate({sx:g},{sy:g}) scale({s:g})">\n{screen}\n</g>')
    ex = 0.5 * s
    o.append(f'<rect id="dev-screen-edge" x="{sx-ex:g}" y="{sy-ex:g}" width="{sw*s+2*ex:g}" height="{sh*s+2*ex:g}" fill="none" stroke="#777" stroke-width="{s:g}"/>')
    if real:
        o.append(f'<rect id="dev-sheen" x="{sx:g}" y="{sy:g}" width="{sw*s:g}" height="{sh*s:g}" fill="url(#g-sheen)"/>')
    return "\n".join(o), (sx, sy)


def spec_svg(orient, variant):
    land = orient == "landscape"
    W, H = (1200, 720) if land else (720, 1200)
    cw, ch = size(orient)
    screen = inner(f"showcase-tab-{orient}.svg")
    real = variant == "photo"
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="{W}" height="{H}" viewBox="0 0 {W} {H}" font-family="DejaVu Sans">']
    o.append(f'<!-- Task 121: real M5Paper frame ({variant}, {orient}). Case 118.6 x 67 x 10 mm, screen 58.32 x 103.68 mm, {K} px/mm (screen = 540x960 px at 1:1). Screen content = docs/ui/showcase-tab-{orient}.svg. Numbers and sources: showcase-device.md. Generated by tools/showcase/device_frame.py. -->')
    o.append("<defs>" + ((DEFS_PHOTO + "\n" + shadow_filter()) if real else "") + "</defs>")
    o.append(f'<rect x="0" y="0" width="{W}" height="{H}" fill="{"url(#g-bg)" if real else "#fff"}"/>')
    svg, _ = frame(orient, variant, (W - cw) / 2, (H - ch) / 2, 1.0, screen)
    o.append(svg)
    o.append("</svg>")
    return "\n".join(o) + "\n"


if __name__ == "__main__":
    for orient in ("landscape", "portrait"):
        for variant in ("clean", "photo"):
            out = UI + f"showcase-device-{variant}-{orient}.svg"
            open(out, "w").write(spec_svg(orient, variant))
            print(out)
