# Regenerate: run gen_showcase_heroes.py, then gen_showcase_composites.py, then tools/render_mockups.sh
# TWICE (first run renders the hero PNGs, second the composites embedding them). showcase.gif is
# then `convert -delay 250 -loop 0 ... -resize 1000x -colors 64` of png/showcase-device-{tabs,popups}.png. Writes docs/ui/showcase-*.svg.
import os
UI = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "docs", "ui") + os.sep
sys_path = os.path.dirname(os.path.abspath(__file__))
import sys; sys.path.insert(0, sys_path)
import device_frame as DF   # real M5Paper frame (task 121)
W, H = 1600, 900
# Portrait 0.66 and landscape 0.82 of the 1:1 size (case 409 x 725 and 900 x 508 px), side by side.
PS, LS = 0.66, 0.82
PX, PY = 100, 40
LX = PX + round(DF.size("portrait", PS)[0]) + 90
LY = round(PY + (DF.size("portrait", PS)[1] - DF.size("landscape", LS)[1]) / 2)
def device(orient, variant, png):
    """Real frame around a rendered 1:1 screen PNG. Returns (svg, (screen_x, screen_y), scale)."""
    x, y, s = (PX, PY, PS) if orient == "portrait" else (LX, LY, LS)
    w, h = (540, 960) if orient == "portrait" else (960, 540)
    img = f'<image x="0" y="0" width="{w}" height="{h}" xlink:href="{png}" href="{png}"/>'
    svg, org = DF.frame(orient, variant, x, y, s, img, rnd=False)
    return svg, org, s
def defs(variant):
    return "<defs>" + ((DF.DEFS_PHOTO + "\n" + DF.shadow_filter(PS)) if variant == "photo" else "") + "</defs>\n"
def background(variant):
    if variant == "photo":
        return f'<rect x="0" y="0" width="{W}" height="{H}" fill="url(#g-bg)"/>\n'
    return (f'<rect x="0" y="0" width="{W}" height="{H}" fill="#ebebe9"/>\n<rect x="0" y="738" width="{W}" height="162" fill="#e0e0de"/>\n'
            f'<rect x="0" y="736" width="{W}" height="2" fill="#d2d2d0"/>\n')
def cap(x, y, t, anchor="middle"):
    return f'<text x="{x}" y="{y}" font-size="26" fill="#555" text-anchor="{anchor}" dominant-baseline="alphabetic">{t}</text>\n'
def comp(variant, p_png, l_png, cp, cl, comment):
    out = (f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="{W}" height="{H}" viewBox="0 0 {W} {H}" font-family="DejaVu Sans">\n<!-- {comment} -->\n'
           f'<g id="w-1"><rect id="w-1-frame" x="0" y="0" width="{W}" height="{H}" fill="none"/></g>\n' + defs(variant) + background(variant))
    s, _, _ = device("portrait", variant, p_png); out += s + "\n"
    out += cap(PX + round(DF.size("portrait", PS)[0]) // 2, 850, cp)
    s2, _, _ = device("landscape", variant, l_png); out += s2 + "\n"
    out += cap(LX + round(DF.size("landscape", LS)[0]) // 2, 850, cl)
    return out + "</svg>\n"

open(UI+"showcase-device.svg", "w").write(comp("photo", "png/showcase-tab-portrait.png", "png/showcase-popup-landscape.png", "Graph widgets on the tab", "Tap a sensor: 24 h history", "Showcase composite (task 073): real M5Paper frame (photo variant, landing page hero)."))
open(UI+"showcase-device-tabs.svg", "w").write(comp("clean", "png/showcase-tab-portrait.png", "png/showcase-tab-landscape.png", "Portrait", "Landscape", "Showcase composite, GIF frame 1."))
open(UI+"showcase-device-popups.svg", "w").write(comp("clean", "png/showcase-popup-portrait.png", "png/showcase-popup-landscape.png", "Portrait", "Landscape", "Showcase composite, GIF frame 2."))
