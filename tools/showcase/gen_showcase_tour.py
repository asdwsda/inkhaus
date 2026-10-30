#!/usr/bin/env python3
"""Generates docs/ui/showcase-tour-*.svg (task 075): feature tour frames, portrait + landscape device
frames side by side (geometry reused from gen_showcase_composites.py), one caption per step, tap rings.
Writes docs/ui/showcase-tour-frames.txt (file<TAB>ms) for build_showcase_tour_gif.sh."""
import os
HERE = os.path.dirname(os.path.abspath(__file__))
src = open(os.path.join(HERE, "gen_showcase_composites.py")).read()
exec(compile(src.split("\nopen(")[0], "composites", "exec"))   # defines UI, W, H, device(), cap()

# ---- derived tab states (PIL patches of the approved PNGs; source mockups are not touched) ----
from PIL import Image, ImageDraw, ImageFont
import numpy as np
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
PNG = UI + "png/"
def ink(img, box):
    a = np.array(img.crop(box).convert("L")).astype(int)
    ys, xs = np.where(abs(a - a[0, 0]) > 90)
    return (box[0]+xs.min(), box[1]+ys.min(), box[0]+xs.max()+1, box[1]+ys.max()+1), img.getpixel(box[:2])
def retext(img, box, old, new):
    (x0, y0, x1, y1), bg = ink(img, box)
    best = min(range(10, 90), key=lambda z: (lambda bb: abs(bb[2]-bb[0]-(x1-x0)) + abs(bb[3]-bb[1]-(y1-y0)))(ImageFont.truetype(FONT, z).getbbox(old)))
    f = ImageFont.truetype(FONT, best); d = ImageDraw.Draw(img)
    d.rectangle(box, fill=bg)
    nb = f.getbbox(new)
    d.text((x0-nb[0], y0-nb[1]), new, font=f, fill=(0, 0, 0) if sum(bg[:3]) > 380 else (255, 255, 255))
# geometry per orientation
G = {"p": dict(ceil=(14, 298, 268, 406), dx=260, lab=(22, 312, 250, 348), cl=(24, 312, 125, 348), t80=(108, 350, 200, 392),
               up=(288, 80, 368, 160), dn=(442, 80, 518, 160), cs=(24, 236, 160, 272), ce=290, ts=(318, 548, 444, 625)),
     "l": dict(ceil=(578, 71, 760, 178), dx=188, lab=(588, 90, 740, 120), cl=(588, 90, 700, 120), t80=(672, 118, 752, 160),
               up=(330, 80, 412, 170), dn=(486, 80, 562, 170), cs=(24, 240, 160, 276), ce=330, ts=(360, 322, 488, 395))}
def tab_state(o, ceiling, shut_open, sp, asleep=False):
    g = G[o]; k = o
    img = Image.open(PNG + ("tab-portrait" if o == "p" else "tab-landscape") + ("-asleep" if asleep else "") + ".png").convert("RGB")
    retext(img, g["t80"], "80%", "75%")
    if not ceiling:
        c = g["ceil"]
        lab = Image.eval(img.crop(g["cl"]).convert("L"), lambda v: 255 - v).convert("RGB")
        img.paste(img.crop((c[0]+g["dx"], c[1], c[2]+g["dx"], c[3])), c[:2])
        ImageDraw.Draw(img).rectangle(g["lab"], fill=(255, 255, 255))
        img.paste(lab, g["cl"][:2])
    if not shut_open:
        ub, _ = ink(img, g["up"]); db, _ = ink(img, g["dn"])
        ImageDraw.Draw(img).rectangle(g["up"], fill=(255, 255, 255))
        fl = img.crop(db).transpose(Image.FLIP_TOP_BOTTOM)
        img.paste(fl, ((ub[0]+ub[2]-fl.width)//2, (ub[1]+ub[3]-fl.height)//2))
        cs = g["cs"]; dy = 116
        txt = img.crop(cs)
        ImageDraw.Draw(img).rectangle((cs[0], cs[1]-dy, g["ce"], cs[3]-dy), fill=(255, 255, 255))
        img.paste(txt, (cs[0], cs[1]-dy))
    if sp:
        retext(img, g["ts"], "22.0\u00b0", "22.5\u00b0")
    return img
def popup_state(o):
    img = tab_state(o, True, False, False)
    br = Image.open(PNG + ("brightness-portrait" if o == "p" else "brightness-landscape") + ".png").convert("RGB")
    if o == "p":
        retext(br, (226, 362, 285, 418), "60", "75")
        d = ImageDraw.Draw(br); d.rounded_rectangle((36, 430, 387, 487), radius=4, fill=(0, 0, 0))
        m = Image.new("L", br.size, 0); ImageDraw.Draw(m).rounded_rectangle((275, 578, 386, 642), radius=8, fill=255)
        inv = Image.eval(br.convert("L"), lambda v: 255 - v).convert("RGB")
        br.paste(inv, (0, 0), m); d.rounded_rectangle((275, 578, 386, 642), radius=8, outline=(0, 0, 0), width=3)
        P = (15, 277, 525, 659)
    else:
        P = (152, 135, 809, 463)
    m = Image.new("L", br.size, 0); md = ImageDraw.Draw(m)
    md.rectangle(P, fill=255); md.rectangle((P[0]+7, P[1]+7, P[2]+7, P[3]+7), fill=255)
    img.paste(br, (0, 0), m)
    return img
def build_states():
    st = {}
    for o, ori in (("p", 0), ("l", 1)):
        for name, a in {"asleep0": tab_state(o, 0, 0, 0, True), "asleep3": tab_state(o, 1, 1, 1, True),
                        "t0": tab_state(o, 0, 0, 0), "t1": tab_state(o, 1, 0, 0), "t2": tab_state(o, 1, 1, 0),
                        "t3": tab_state(o, 1, 1, 1), "pop": popup_state(o)}.items():
            fn = f"png/showcase-tour-src-{name}-{o}.png"; a.save(UI + fn); st.setdefault(name, [None, None])[ori] = fn
    for o, ori, bx in (("portrait", 0, (108, 586, 200, 616)), ("landscape", 1, (864, 120, 935, 160))):
        im = Image.open(PNG + f"showcase-tab-{o}.png").convert("RGB"); retext(im, bx, "80%", "75%")
        fn = f"png/showcase-tour-src-gtab-{o[0]}.png"; im.save(UI + fn); st.setdefault("gtab", [None, None])[ori] = fn
    for o, ori, bx, tabs in (("portrait", 0, (28, 92, 170, 118), ((16, 892, 134, 948), (147, 892, 264, 948))),
                             ("landscape", 1, (28, 95, 170, 122), ((172, 4, 292, 60), (304, 4, 424, 60)))):
        im = Image.open(UI + f"png/showcase-tour-src-gtab-{o[0]}.png").convert("RGB")
        retext(im, bx, "Living room", "Whole house")      # tapped tab 2 = Whole house, shown highlighted
        for t in tabs:
            m = Image.new("L", im.size, 0); ImageDraw.Draw(m).rounded_rectangle(t, radius=10, fill=255)
            im.paste(Image.eval(im.convert("L"), lambda v: 255 - v).convert("RGB"), (0, 0), m)
            ImageDraw.Draw(im).rounded_rectangle(t, radius=10, outline=(0, 0, 0), width=2)
        im.save(UI + f"png/showcase-tour-src-gtab-{o[0]}.png")
    for o, ori, fw, cfg, clr, bdg, vx in (("portrait", 0, (170, 512, 260, 542), (155, 698, 520, 758), (0, 762, 540, 812), (396, 4, 530, 50), 174),
                                          ("landscape", 1, (644, 72, 760, 108), (628, 290, 958, 358), (480, 364, 958, 412), (816, 4, 950, 46), 646)):
        im = Image.open(PNG + f"settings-{o}.png").convert("RGB"); d = ImageDraw.Draw(im)
        retext(im, fw, "v0.3.1", "1.2.0")
        # value font size taken from the Wi-Fi value ("HomeNet-5G")
        wb, _ = ink(im, (vx - 4, 640, vx + 200, 690)) if o == "portrait" else ink(im, (vx - 4, 236, vx + 200, 280))
        fz = min(range(14, 40), key=lambda z: abs(ImageFont.truetype(FONT, z).getbbox("HomeNet-5G")[2] - (wb[2] - wb[0])))
        fz = 24 if o == "portrait" else fz
        f = ImageFont.truetype(FONT, fz)
        d.rectangle(cfg, fill=(255, 255, 255)); d.rectangle(clr, fill=(255, 255, 255)); d.rectangle(bdg, fill=(255, 255, 255))
        cy = (cfg[1] + cfg[3]) // 2 + (0 if o == "portrait" else -2)
        d.text((vx, cy), "ok", font=f, fill=(0, 0, 0), anchor="lm")
        fn = f"png/showcase-tour-src-set-{o[0]}.png"; im.save(UI + fn); st.setdefault("set", [None, None])[ori] = fn
    st["hist"] = ["png/showcase-popup-portrait.png", "png/showcase-popup-landscape.png"]
    return st
S = build_states()
# tap targets in screen px: (portrait (x,y), landscape (x,y))
T = {"wake": ((270, 480), (480, 270)), "ceiling": ((140, 352), (668, 124)),
     "shutter": ((328, 120), (372, 124)), "climate": ((480, 584), (524, 356)),
     "tabslot": ((205, 920), (364, 32)), "graph": ((270, 150), (198, 180)),
     "network": ((270, 333), (247, 342))}
CAP = ["Asleep: the last state stays on screen", "Wake: fresh states",
       "Tap a light: on", "Long press: brightness", "Shutters: open, stop or close",
       "Climate: set the temperature", "Graph widgets on the tab", "Long press a sensor: 24 h history",
       "Settings on the device", "Back to sleep"]
# frames: (step, screen, ring key or None, ms); a tap frame is followed by its "after" frame
F = [(1, "asleep0", None, 2400), (2, "asleep0", "wake", 700), (2, "t0", None, 1800),
     (3, "t0", "ceiling", 1000), (3, "t1", None, 1500), (4, "t1", ("ceiling", 1), 900), (4, "pop", None, 1800),
     (5, "t1", "shutter", 1000), (5, "t2", None, 1500), (6, "t2", "climate", 1000), (6, "t3", None, 1500),
     (7, "t3", "tabslot", 700), (7, "gtab", None, 1800), (8, "gtab", ("graph", 1), 900), (8, "hist", None, 1800),
     (9, "set", "network", 2400), (10, "asleep3", None, 2400)]

def ring(cx, cy, long):
    s = f'<circle cx="{cx}" cy="{cy}" r="27" fill="#000" fill-opacity="0.28" stroke="#fff" stroke-width="11"/>\n'
    s += f'<circle cx="{cx}" cy="{cy}" r="27" fill="none" stroke="#000" stroke-width="5"/>\n'
    s += f'<circle cx="{cx}" cy="{cy}" r="8" fill="#000"/>\n'
    if long:
        s += f'<circle cx="{cx}" cy="{cy}" r="41" fill="none" stroke="#000" stroke-width="4" stroke-dasharray="9 7"/>\n'
    return s

PX, PY, PS, LX, LY, LS, PAD = 111, 50, 0.70, 611, 150, 0.85, 26
frames = []
for i, (step, scr, tap, ms) in enumerate(F):
    p, l = S[scr]
    out = (f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="{W}" height="{H}" viewBox="0 0 {W} {H}" font-family="DejaVu Sans">\n'
           f'<!-- Tour frame {i+1}, step {step} (task 075). Generated by tools/showcase/gen_showcase_tour.py. -->\n'
           f'<g id="w-1"><rect id="w-1-frame" x="0" y="0" width="{W}" height="{H}" fill="none"/></g>\n'
           f'<rect x="0" y="0" width="{W}" height="{H}" fill="#ebebe9"/>\n<rect x="0" y="738" width="{W}" height="162" fill="#e0e0de"/>\n<rect x="0" y="736" width="{W}" height="2" fill="#d2d2d0"/>\n')
    s, bw, bh = device(PX, PY, 540, 960, PS, p, "p"); out += s
    s2, bw2, bh2 = device(LX, LY, 960, 540, LS, l, "l"); out += s2
    if tap:
        key, long = (tap if isinstance(tap, tuple) else (tap, 0))
        (a, b), (c, d) = T[key]
        out += ring(PX+PAD+round(a*PS), PY+PAD+round(b*PS), long) + ring(LX+PAD+round(c*LS), LY+PAD+round(d*LS), long)
    cx = (PX + LX + bw2) // 2
    out += f'<text x="{cx}" y="826" font-size="38" fill="#111" text-anchor="middle">{CAP[step-1]}</text>\n'
    dots = "".join(f'<circle cx="{cx-117+26*k}" cy="866" r="{7 if k+1==step else 5}" fill="{"#111" if k+1==step else "#a8a8a6"}"/>' for k in range(10))
    out += dots + "\n</svg>\n"
    frames.append((f"showcase-tour-{i+1:02d}.svg", ms))
    open(UI + frames[-1][0], "w").write(out)
open(UI + "showcase-tour-frames.txt", "w").write("".join(f"{n}\t{ms}\n" for n, ms in frames))
print(len(frames), "frames")
