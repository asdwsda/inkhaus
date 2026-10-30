#!/usr/bin/env python3
# Regenerate: run gen_showcase_heroes.py, then gen_showcase_composites.py, then tools/render_mockups.sh
# TWICE (first run renders the hero PNGs, second the composites embedding them). showcase.gif is
# then `convert -delay 250 -loop 0` of png/showcase-device-{tabs,popups}.png. Writes docs/ui/showcase-*.svg.
"""Generates docs/ui/showcase-*.svg (task 073). Data: simulated living room (thermostat + window airing)."""
import math, random, re, sys
import os
UI = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "docs", "ui") + os.sep
rnd = random.Random(73)

# ---------------- data ----------------
START = 14 + 5/60            # window starts 14:05 yesterday, ends 14:05 today
STEP = 2/60                  # 2 min
N = int(24/STEP)
def tout(h): return 6.8 + 4.0*math.cos(2*math.pi*(h-15)/24)
HEAT = [(4.6, 7.0), (12.3, 13.0), (17.2, 18.8), (22.2, 23.2)]
def sim():
    T = 20.2; P = 0.0; out = []; ons = []
    for k in range(-int(24/STEP), N):
        h = (START + k*STEP) % 24
        on = any(a <= h < b for a, b in HEAT)
        P += (on - P)*STEP/0.45
        window = 10.0 <= h < 10.3
        sun = 0.3*max(0, math.sin(math.pi*(h-8)/9)) if 8 <= h < 17 else 0
        loss = 0.025*(T - tout(h))*(16 if window else 1)
        T += (1.6*P + sun - loss)*STEP
        if k >= 0:
            out.append(T); ons.append(on)
    return out, ons
T, ONS = sim()
def noisy(vals, amp):
    return [[v + rnd.gauss(0, amp), v + rnd.gauss(0, amp), v] for v in vals]
TS = noisy(T, 0.14)
TO = [tout((START + k*STEP) % 24) for k in range(N)]
OS = noisy(TO, 0.0)

def cols(samples, pw):
    """per pixel column: (min, max, last)"""
    res = []
    for c in range(pw):
        a, b = int(c*len(samples)/pw), max(int((c+1)*len(samples)/pw), int(c*len(samples)/pw)+1)
        vs = [v for s in samples[a:b] for v in s]
        res.append((min(vs), max(vs), samples[b-1][-1]))
    return res

def spans(on, pw):
    out = []; start = None
    for k, v in enumerate(on + [False]):
        if v and start is None: start = k
        if not v and start is not None:
            out.append((start*pw/len(on), k*pw/len(on))); start = None
    return out

def fmt(v): return f"{v:.1f}"

# ---------------- graph pieces (coordinates in a given origin) ----------------
GRID, MID = "#d0d0d0", "#808080"
def line_plot(samples, x0, y0, pw, ph, ymin, ymax, pid):
    cs = cols(samples, pw)
    def Y(v): return y0 + ph*(1 - (v - ymin)/(ymax - ymin))
    def cl(y): return min(max(y, y0), y0+ph-1)
    top = " ".join(f"{x0+i:.1f},{cl(Y(c[1])):.1f}" for i, c in enumerate(cs))
    bot = " ".join(f"{x0+i:.1f},{cl(Y(c[0])):.1f}" for i, c in reversed(list(enumerate(cs))))
    line = " ".join(f"{x0+i+0.5:.1f},{cl(Y(c[2])):.1f}" for i, c in enumerate(cs) if i % 2 == 0 or i == pw-1)
    last = cs[-1][2]
    return (f'<polygon id="g-{pid}-band" points="{top} {bot}" fill="{GRID}"/>\n',
            f'<polyline id="g-{pid}-line" points="{line}" fill="none" stroke="#000" stroke-width="2" stroke-linejoin="round"/>\n'
            f'<circle id="g-{pid}-dot" cx="{x0+pw:.1f}" cy="{cl(Y(last)):.1f}" r="5" fill="#000"/>\n')

def tx(id_, x, y, s, size=18, anchor="start", fill="#000"):
    return f'<text id="{id_}" x="{x}" y="{y}" font-size="{size}" fill="{fill}" text-anchor="{anchor}" dominant-baseline="alphabetic">{s}</text>\n'

def xpos_of_hour(h, pw):  # hour of day -> offset within the plot
    d = (h - START) % 24
    return d*pw/24

# ---------------- widgets ----------------
def header(wid, W, name, val, unit):
    return (tx(f"w-{wid}-name", 12, 44, name, 24) +
            f'<text id="w-{wid}-value" x="{W-12}" y="50" font-size="40" fill="#000" text-anchor="end" dominant-baseline="alphabetic">{val}<tspan font-size="24" dx="2">{unit}</tspan></text>\n')

def frame(wid, W, H, fill="#fff"):
    return f'<rect id="w-{wid}-frame" x="1" y="1" width="{W-2}" height="{H-2}" rx="8" fill="{fill}" stroke="#000" stroke-width="2"/>\n'

def line_widget(wid, x, y, W, name, samples, ymin, ymax, ticks, decimals=1, unit="°C"):
    pw, ph = W-84, 86
    band, line = line_plot(samples, 60, 78, pw, ph, ymin, ymax, f"w-{wid}")
    s = frame(wid, W, 220) + header(wid, W, name, fmt(samples[-1][-1]), unit)
    s += f'<rect id="g-w-{wid}-h0" x="60" y="78" width="{pw}" height="1" fill="{GRID}"/>\n'
    s += f'<rect id="g-w-{wid}-h1" x="60" y="121" width="{pw}" height="1" fill="{GRID}"/>\n'
    for i, (h, lab) in enumerate(ticks):
        tx_ = 60 + round(xpos_of_hour(h, pw))
        s += f'<rect id="g-w-{wid}-v{i}" x="{tx_}" y="78" width="1" height="{ph}" fill="{GRID}"/>\n'
    s += band
    s += f'<rect id="g-w-{wid}-axl" x="59" y="74" width="2" height="{ph+5}" fill="#000"/>\n'
    s += f'<rect id="g-w-{wid}-axb" x="59" y="{78+ph}" width="{pw+2}" height="2" fill="#000"/>\n'
    s += line
    s += tx(f"w-{wid}-yt", 52, 84, fmt(ymax), 18, "end") + tx(f"w-{wid}-yb", 52, 170, fmt(ymin), 18, "end")
    for i, (h, lab) in enumerate(ticks):
        tx_ = 60 + round(xpos_of_hour(h, pw))
        s += f'<rect id="g-w-{wid}-t{i}" x="{tx_-1}" y="164" width="2" height="6" fill="{MID}"/>\n'
        s += tx(f"w-{wid}-xl{i}", tx_, 194, lab, 18, "middle")
    return f'<g id="w-{wid}" transform="translate({x},{y})">\n{s}</g>\n'

def bar_widget(wid, x, y, W, name, vals, labels_x, show_vals, ymax, tick_labels, total_str, unit="kWh"):
    pw, ph = W-84, 86; n = len(vals); slot = pw//n; bw = slot-20
    s = frame(wid, W, 220) + header(wid, W, name, total_str, unit)
    s += f'<rect id="g-w-{wid}-h0" x="60" y="78" width="{pw}" height="1" fill="{GRID}"/>\n'
    s += f'<rect id="g-w-{wid}-h1" x="60" y="121" width="{pw}" height="1" fill="{GRID}"/>\n'
    for i, v in enumerate(vals):
        bh = round(ph*v/ymax); bx = 60 + i*slot + 10; by = 78+ph-bh
        if i == n-1:
            s += f'<rect id="g-w-{wid}-bar{i}" x="{bx+1}" y="{by+1}" width="{bw-2}" height="{bh-1}" fill="{MID}" stroke="#000" stroke-width="2"/>\n'
        else:
            s += f'<rect id="g-w-{wid}-bar{i}" x="{bx}" y="{by}" width="{bw}" height="{bh}" fill="#000"/>\n'
        if show_vals:
            s += tx(f"w-{wid}-bv{i}", bx+bw//2, by-14+6, fmt(v), 18, "middle")
    s += f'<rect id="g-w-{wid}-axl" x="59" y="74" width="2" height="{ph+5}" fill="#000"/>\n'
    s += f'<rect id="g-w-{wid}-axb" x="59" y="{78+ph}" width="{pw+2}" height="2" fill="#000"/>\n'
    s += tx(f"w-{wid}-yt", 52, 84, str(ymax), 18, "end") + tx(f"w-{wid}-yb", 52, 170, "0", 18, "end")
    for i, lab in tick_labels:
        s += tx(f"w-{wid}-xl{i}", 60 + i*slot + 10 + bw//2, 194, lab, 18, "middle")
    return f'<g id="w-{wid}" transform="translate({x},{y})">\n{s}</g>\n'

def compact_widget(wid, x, y, W, kind, name, valstr, unit, samples=None, on=None, ymin=None, ymax=None, ticks=None):
    sx, sw = (228, 263) if W > 400 else (164, 183)
    s = frame(wid, W, 104) + tx(f"w-{wid}-name", 12, 44, name, 24)
    s += f'<text id="w-{wid}-value" x="12" y="77" font-size="24" fill="#000" dominant-baseline="alphabetic">{valstr}' + (f'<tspan font-size="24" dx="2">{unit}</tspan>' if unit else '') + '</text>\n'
    if kind == "line":
        band, line = line_plot(samples, sx, 20, sw, 62, ymin, ymax, f"w-{wid}")
        s += band + line
    else:
        ly = 31
        s += f'<rect id="g-w-{wid}-lane" x="{sx}" y="{ly}" width="{sw}" height="40" fill="#fff" stroke="#000" stroke-width="2"/>\n'
        for i, (a, b) in enumerate(spans(on, sw-4)):
            s += f'<rect id="g-w-{wid}-on{i}" x="{sx+2+a:.1f}" y="{ly+2}" width="{max(b-a,3):.1f}" height="36" fill="#000"/>\n'
    s += f'<rect id="g-w-{wid}-ax" x="{sx}" y="82" width="{sw+2}" height="2" fill="#000"/>\n'
    return f'<g id="w-{wid}" transform="translate({x},{y})">\n{s}</g>\n'

def cell(wid, x, y, W, name, icon, state, on):
    f, t = ("#000", "#fff") if on else ("#fff", "#000")
    return (f'<g id="w-{wid}" transform="translate({x},{y})">\n'
            f'<rect id="w-{wid}-frame" x="1" y="1" width="{W-2}" height="102" rx="8" fill="{f}" stroke="#000" stroke-width="2"/>\n'
            f'<text id="w-{wid}-name" x="12" y="38" font-size="24" fill="{t}" dominant-baseline="alphabetic">{name}</text>\n'
            f'<use id="w-{wid}-icon" xlink:href="#{icon}" href="#{icon}" x="12" y="49" width="40" height="40" color="{t}"/>\n'
            f'<text id="w-{wid}-state" x="58" y="77" font-size="24" fill="{t}" dominant-baseline="alphabetic">{state}</text>\n</g>\n')

def thermostat(wid, x, y, W):
    cx = W-... if False else None

# ---------------- page parts ----------------
P = open(UI+"history-widget-portrait.svg").read().split("\n")
L = open(UI+"history-widget-landscape.svg").read().split("\n")
DEFS = "\n".join(P[2:33]).split("<defs>")[1].split("</defs>")[0]
DEFS += '    <symbol id="i-close" viewBox="0 0 24 24"><path d="M5 5 L19 19 M19 5 L5 19" fill="none" stroke="currentColor" stroke-width="3"/></symbol>\n'
CH_P_TOP = "\n".join(P[34:46]); CH_P_TAB = "\n".join(P[123:136])
CH_L = "\n".join(L[33:61])
assert "w-1" not in CH_P_TOP + CH_P_TAB + CH_L, "chrome slice wrong"
def head(w, h, comment): return (f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="{w}" height="{h}" viewBox="0 0 {w} {h}" font-family="DejaVu Sans">\n'
                                 f'<!-- {comment} -->\n<defs>{DEFS}</defs>\n')

# values
cur = T[-1]; lo = min(T); hi = max(T); out_now = TO[-1]
PORT_TICKS = [(18, "18:00"), (0, "00:00"), (6, "06:00"), (12, "12:00")]
LAND_TICKS = [(0, "00:00"), (12, "12:00")]
YMIN, YMAX = 19.0, 23.0
ENERGY = [6.2, 7.8, 5.1, 9.6, 8.3, 7.0, 5.9]
DAYS = ["Sat", "Sun", "Mon", "Tue", "Wed", "Thu", "Today"]
heat_min = sum(ONS)*2
oy = (min(TO)-0.4, max(TO)+0.4)

def tab_landscape(ids=True):
    s = CH_L + "\n"
    s += line_widget(1, 16, 72, 364, "Living room", TS, YMIN, YMAX, LAND_TICKS)
    s += bar_widget(2, 392, 72, 364, "Energy", ENERGY, None, False, 12, [(0, "Sat"), (2, "Mon"), (4, "Wed"), (6, "Today")], "5.9", "kWh")
    s += cell(3, 768, 72, 176, "Ceiling", "i-bulb", "On 80%", True)
    s += cell(4, 768, 188, 176, "Floor lamp", "i-bulb-off", "Off", False)
    s += cell(5, 16, 304, 176, "Fan", "i-plug", "Off", False)
    s += cell(6, 204, 304, 176, "Window", "i-window-on", "Open", False)
    s += compact_widget(7, 392, 304, 364, "step", "Heating", "On" if ONS[-1] else "Off", "", on=ONS)
    s += cell(8, 768, 304, 176, "Night mode", "i-auto", "On", True)
    s += compact_widget(9, 16, 420, 364, "line", "Outside", fmt(out_now), "°C", samples=OS, ymin=oy[0], ymax=oy[1])
    s += compact_widget(10, 392, 420, 364, "line", "Bedroom", "19.8", "°C", samples=noisy([19.6+0.5*math.sin(2*math.pi*(k*STEP-3)/24)+0.0 for k in range(N)], 0.05), ymin=18.9, ymax=20.6)
    s += cell(11, 768, 420, 176, "Front door", "i-window-off", "Closed", False)
    return s

def tab_portrait():
    s = CH_P_TOP + "\n"
    s += line_widget(1, 16, 68, 508, "Living room", TS, YMIN, YMAX, PORT_TICKS)
    s += bar_widget(2, 16, 300, 508, "Energy per day", ENERGY, None, True, 12, list(enumerate(DAYS)), "5.9", "kWh")
    s += cell(3, 16, 532, 248, "Ceiling", "i-bulb", "On 80%", True)
    s += cell(4, 276, 532, 248, "Night mode", "i-auto", "On", True)
    s += compact_widget(5, 16, 648, 508, "step", "Heating", "On" if ONS[-1] else "Off", "", on=ONS)
    s += compact_widget(6, 16, 764, 508, "line", "Outside", fmt(out_now), "°C", samples=OS, ymin=oy[0], ymax=oy[1])
    return s + CH_P_TAB + "\n"

# ---------------- popups ----------------
def strip_ids(s): return re.sub(r' id="[^"]*"', "", s)
def strip(px, y, pw):
    return "".join(f'<rect id="g-heat{i}" x="{px+a:.1f}" y="{y}" width="{max(b-a,3):.1f}" height="8" fill="#000"/>\n' for i, (a, b) in enumerate(spans(ONS, pw)))

def popup_land():
    px, py, pw, ph = 116, 186, 764, 240
    ymin, ymax = 19.0, 23.0
    bandp, linep = line_plot(TS, px, py, pw, ph, ymin, ymax, "p")
    s = f'<rect id="popup-shadow" x="46" y="92" width="880" height="424" rx="8" fill="#000"/>\n<g id="w-1">\n'
    s += '<rect id="w-1-frame" x="40" y="86" width="880" height="424" rx="8" fill="#fff" stroke="#000" stroke-width="3"/>\n'
    s += tx("w-1-name", 56, 142, "Living room temp.", 24)
    s += '<rect id="w-1-close-btn" x="840" y="102" width="64" height="64" rx="8" fill="#fff" stroke="#000" stroke-width="2"/>\n'
    s += '<use id="w-1-close-icon" xlink:href="#i-close" href="#i-close" x="852" y="114" width="40" height="40" color="#000"/>\n'
    s += f'<text id="w-1-value" x="372" y="148" font-size="40" fill="#000" dominant-baseline="alphabetic">{fmt(cur)}<tspan font-size="24" dx="8">°C</tspan></text>\n'
    s += tx("w-1-range", 824, 127, "Last 24 h", 18, "end") + tx("w-1-range2", 824, 153, f"min {fmt(lo)}  max {fmt(hi)}", 18, "end")
    for yy in (py, py+120, py+240): s += f'<line x1="{px}" y1="{yy}" x2="{px+pw}" y2="{yy}" stroke="{GRID}" stroke-width="1"/>\n'
    hours = [15, 18, 21, 0, 3, 6, 9, 12]
    xs = [px + round(xpos_of_hour(h, pw)) for h in hours]
    for x in xs: s += f'<line x1="{x}" y1="{py}" x2="{x}" y2="{py+ph}" stroke="{GRID}" stroke-width="1"/>\n'
    # heating strip under the line (step lane analogue): thin MID blocks above the x axis
    s += bandp
    s += f'<line x1="{px-1}" y1="{py-4}" x2="{px-1}" y2="{py+ph+1}" stroke="#000" stroke-width="2"/>\n<line x1="{px-1}" y1="{py+ph+1}" x2="{px+pw}" y2="{py+ph+1}" stroke="#000" stroke-width="2"/>\n'
    for x in xs: s += f'<line x1="{x}" y1="{py+ph+2}" x2="{x}" y2="{py+ph+8}" stroke="{MID}" stroke-width="2"/>\n'
    s += strip(px, py+ph-14, pw)
    s += linep
    for i, v in enumerate((ymin, 21.0, ymax)): s += tx(f"w-1-yl{i}", 108, py+ph-120*i+6, fmt(v), 18, "end")
    for i, h in enumerate(hours): s += tx(f"w-1-xl{i}", xs[i], 454, f"{h:02d}:00", 18, "middle")
    s += tx("w-1-note", 56, 488, "updated 14:05", 18)
    s += f'<rect id="w-1-leg-sw" x="792" y="476" width="24" height="12" fill="{GRID}"/>\n' + tx("w-1-leg", 904, 488, "min-max", 18, "end")
    s += '<rect id="w-1-heat-sw" x="600" y="480" width="24" height="8" fill="#000"/>\n' + tx("w-1-heat", 632, 488, "heating on", 18)
    return s + "</g>\n"

def popup_port():
    px, py, pw, ph = 92, 372, 392, 264
    ymin, ymax = 19.0, 23.0
    bandp, linep = line_plot(TS, px, py, pw, ph, ymin, ymax, "p")
    s = '<rect id="popup-shadow" x="22" y="218" width="508" height="512" rx="8" fill="#000"/>\n<g id="w-1">\n'
    s += '<rect id="w-1-frame" x="16" y="212" width="508" height="512" rx="8" fill="#fff" stroke="#000" stroke-width="3"/>\n'
    s += tx("w-1-name", 32, 268, "Living room temp.", 24)
    s += '<rect id="w-1-close-btn" x="444" y="228" width="64" height="64" rx="8" fill="#fff" stroke="#000" stroke-width="2"/>\n'
    s += '<use id="w-1-close-icon" xlink:href="#i-close" href="#i-close" x="456" y="240" width="40" height="40" color="#000"/>\n'
    s += f'<text id="w-1-value" x="32" y="342" font-size="40" fill="#000">{fmt(cur)}<tspan font-size="24" dx="8">°C</tspan></text>\n'
    s += tx("w-1-range", 508, 321, "Last 24 h", 18, "end") + tx("w-1-range2", 508, 347, f"min {fmt(lo)}  max {fmt(hi)}", 18, "end")
    for yy in (py, py+132, py+264): s += f'<line x1="{px}" y1="{yy}" x2="{px+pw}" y2="{yy}" stroke="{GRID}" stroke-width="1"/>\n'
    hours = [18, 0, 6, 12]
    xs = [px + round(xpos_of_hour(h, pw)) for h in hours]
    for x in xs: s += f'<line x1="{x}" y1="{py}" x2="{x}" y2="{py+ph}" stroke="{GRID}" stroke-width="1"/>\n'
    s += bandp
    s += f'<line x1="{px-1}" y1="{py-4}" x2="{px-1}" y2="{py+ph+1}" stroke="#000" stroke-width="2"/>\n<line x1="{px-1}" y1="{py+ph+1}" x2="{px+pw}" y2="{py+ph+1}" stroke="#000" stroke-width="2"/>\n'
    for x in xs: s += f'<line x1="{x}" y1="{py+ph+2}" x2="{x}" y2="{py+ph+8}" stroke="{MID}" stroke-width="2"/>\n'
    s += strip(px, py+ph-14, pw)
    s += linep
    for i, v in enumerate((ymin, 21.0, ymax)): s += tx(f"w-1-yl{i}", 84, py+ph-132*i+6, fmt(v), 18, "end")
    for i, h in enumerate(hours): s += tx(f"w-1-xl{i}", xs[i], 664, f"{h:02d}:00", 18, "middle")
    s += tx("w-1-note", 32, 704, "updated 14:05", 18)
    s += f'<rect id="w-1-leg-sw" x="396" y="692" width="24" height="12" fill="{GRID}"/>\n' + tx("w-1-leg", 508, 704, "min-max", 18, "end")
    s += '<rect id="w-1-heat-sw" x="176" y="696" width="24" height="8" fill="#000"/>\n' + tx("w-1-heat", 208, 704, "heating", 18)
    return s + "</g>\n"

def write(name, w, h, comment, body):
    open(UI+name, "w").write(head(w, h, comment) + f'<rect x="0" y="0" width="{w}" height="{h}" fill="#fff"/>\n' + body + "</svg>\n")

if __name__ == "__main__":
    write("showcase-tab-landscape.svg", 960, 540, "Showcase hero (task 073): landscape tab with graph widgets: line+band, bar, compact step and line. Generated; data is a simulated thermostat day.", tab_landscape())
    write("showcase-tab-portrait.svg", 540, 960, "Showcase hero (task 073): portrait tab with graph widgets.", tab_portrait())
    write("showcase-popup-landscape.svg", 960, 540, "Showcase hero (task 073): history popup over the landscape tab.", strip_ids(tab_landscape()) + popup_land())
    write("showcase-popup-portrait.svg", 540, 960, "Showcase hero (task 073): history popup over the portrait tab.", strip_ids(tab_portrait()) + popup_port())
    print("cur", fmt(cur), "min", fmt(lo), "max", fmt(hi), "out", fmt(out_now), "heat on min", heat_min, "now on", ONS[-1], "spans", [(round((START+a*24/200)%24,1)) for a,b in spans(ONS,200)])
