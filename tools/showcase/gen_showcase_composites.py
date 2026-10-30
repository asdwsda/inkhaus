# Regenerate: run gen_showcase_heroes.py, then gen_showcase_composites.py, then tools/render_mockups.sh
# TWICE (first run renders the hero PNGs, second the composites embedding them). showcase.gif is
# then `convert -delay 250 -loop 0 ... -resize 1000x -colors 64` of png/showcase-device-{tabs,popups}.png. Writes docs/ui/showcase-*.svg.
import os
UI = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "docs", "ui") + os.sep
W, H = 1600, 900
def device(x, y, sw, sh, scale, png, kind):
    w, h = round(sw*scale), round(sh*scale)
    pad = 26
    bw, bh = w+2*pad, h+2*pad
    s = f'<rect x="{x+10}" y="{y+14}" width="{bw}" height="{bh}" rx="34" fill="#c4c4c2"/>\n'
    s += f'<rect x="{x+6}" y="{y+8}" width="{bw}" height="{bh}" rx="34" fill="#d2d2d0"/>\n'
    s += f'<rect x="{x}" y="{y}" width="{bw}" height="{bh}" rx="34" fill="#f7f7f5" stroke="#a8a8a6" stroke-width="2"/>\n'
    s += f'<rect x="{x+6}" y="{y+6}" width="{bw-12}" height="{bh-12}" rx="28" fill="none" stroke="#e0e0de" stroke-width="2"/>\n'
    s += f'<rect x="{x+pad-3}" y="{y+pad-3}" width="{w+6}" height="{h+6}" rx="3" fill="#2a2a2a"/>\n'
    s += f'<image x="{x+pad}" y="{y+pad}" width="{w}" height="{h}" xlink:href="{png}" href="{png}"/>\n'
    # rocker on the right side (portrait) or top edge (landscape: device rotated, rocker at the top-right edge)
    if kind == "p":
        s += f'<rect x="{x+bw-2}" y="{y+170}" width="7" height="120" rx="3" fill="#e4e4e2" stroke="#a8a8a6" stroke-width="2"/>\n'
        s += f'<rect x="{x+bw-2}" y="{y+224}" width="7" height="12" rx="2" fill="#a8a8a6"/>\n'
    else:
        s += f'<rect x="{x+bw-290}" y="{y-5}" width="120" height="7" rx="3" fill="#e4e4e2" stroke="#a8a8a6" stroke-width="2"/>\n'
        s += f'<rect x="{x+bw-236}" y="{y-5}" width="12" height="7" rx="2" fill="#a8a8a6"/>\n'
    return s, bw, bh
def cap(x, y, t, anchor="middle"):
    return f'<text x="{x}" y="{y}" font-size="26" fill="#555" text-anchor="{anchor}" dominant-baseline="alphabetic">{t}</text>\n'
def comp(name, p_png, l_png, cp, cl, comment):
    out = (f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="{W}" height="{H}" viewBox="0 0 {W} {H}" font-family="DejaVu Sans">\n<!-- {comment} -->\n'
           f'<g id="w-1"><rect id="w-1-frame" x="0" y="0" width="{W}" height="{H}" fill="none"/></g>\n'
           f'<rect x="0" y="0" width="{W}" height="{H}" fill="#ebebe9"/>\n<rect x="0" y="738" width="{W}" height="162" fill="#e0e0de"/>\n<rect x="0" y="736" width="{W}" height="2" fill="#d2d2d0"/>\n')
    s, bw, bh = device(120, 50, 540, 960, 0.70, p_png, "p")
    out += s; out += cap(120+bw//2, 868 if False else 850, cp)
    s2, bw2, bh2 = device(720, 150, 960, 540, 0.85, l_png, "l")
    out += s2; out += cap(720+bw2//2, 850, cl)
    return out + "</svg>\n"
open(UI+"showcase-device.svg", "w").write(comp("a", "png/showcase-tab-portrait.png", "png/showcase-popup-landscape.png", "Graph widgets on the tab", "Long press: 24 h history", "Showcase composite (task 073): light device frames around the rendered mockups."))
open(UI+"showcase-device-tabs.svg", "w").write(comp("b", "png/showcase-tab-portrait.png", "png/showcase-tab-landscape.png", "Portrait", "Landscape", "Showcase composite, GIF frame 1."))
open(UI+"showcase-device-popups.svg", "w").write(comp("c", "png/showcase-popup-portrait.png", "png/showcase-popup-landscape.png", "Portrait", "Landscape", "Showcase composite, GIF frame 2."))
