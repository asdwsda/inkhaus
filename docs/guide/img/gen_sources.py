import re, sys
import os
R=os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','..')+'/'
OUT=R+'guide/img/src/'
# Writes the fragment sources from the accepted mockups in docs/ui/ (geometry reused 1:1). Run: python3 -I docs/guide/img/gen_sources.py
tab=open(R+'ui/tab-landscape.svg').read()
asl=open(R+'ui/tab-landscape-asleep.svg').read()
sens=open(R+'ui/sensor-details.svg').read()
syms=''.join(re.findall(r'<symbol.*?</symbol>',tab,re.S))
syms+=''.join(s for s in re.findall(r'<symbol.*?</symbol>',asl,re.S) if 'i-power' in s or 'i-refresh' in s)
syms=re.sub(r'(<symbol id="i-flame")','\\1',syms)
HDR='<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="%d" height="%d" viewBox="0 0 %d %d" font-family="DejaVu Sans">\n'
def grp(src,n):
    m=re.search(r'<g id="w-%d" transform="translate\([\d.]+,[\d.]+\)">(.*?)</g>'%n,src,re.S)
    return m.group(1)
def place(src,n,newn,x,y):
    body=grp(src,n).replace('w-%d-'%n,'w-%d-'%newn)
    return '<g id="w-%d" transform="translate(%d,%d)">%s</g>\n'%(newn,x,y,body)
def txt(i,x,y,s,size=18,fill='#000',anchor='start',extra=''):
    s=s.replace('&','&amp;').replace('<','&lt;')
    return '<text id="%s" x="%s" y="%s" font-size="%d" fill="%s" text-anchor="%s" %s>%s</text>\n'%(i,x,y,size,fill,anchor,extra,s)
def snippet(x,y,w,lines,tag='config.yaml'):
    lh=26; h=22+len(lines)*lh+14+24
    o='<rect id="sn-box" x="%d" y="%d" width="%d" height="%d" rx="8" fill="#f2f2f2" stroke="#888" stroke-width="2"/>\n'%(x,y,w,h)
    o+=txt('sn-tag',x+16,y+26,tag,16,'#444')
    for k,l in enumerate(lines):
        col='#444' if l.strip().startswith('#') else '#000'
        o+=txt('sn-l%d'%k,x+16,y+58+k*lh,l,20,col,extra='font-family="DejaVu Sans Mono" xml:space="preserve"')
    return o,h
def fragment(name,widgets,labels,lines,W,note=None):
    pad=20; wy=pad
    body=widgets
    for i,(lx,ly,s) in enumerate(labels): body+=txt('lb-%d'%i,lx,ly,s,18,'#444',extra='')
    sy=wy+ max(ly for _,ly,_ in labels)-wy+24 if labels else wy
    sn,h=snippet(pad,sy,W-2*pad,lines)
    H=sy+h+pad
    svg=HDR%(W,H,W,H)+'<defs>'+syms+'</defs>\n<rect width="%d" height="%d" fill="#fff"/>\n'%(W,H)+body+sn+'</svg>\n'
    open(OUT+name+'.svg','w').write(svg)

# gallery 1: light on/off (1x1 landscape cells 176 wide)
w=place(tab,3,1,20,20)+place(tab,4,2,208,20)
fragment('gallery-light-on-off',w,[(20,150,'on (filled)'),(208,150,'off (outline)')],
 ['- entity: light.living_room_ceiling','  name: Ceiling','- entity: light.floor_lamp','  name: Floor lamp'],520)
# gallery 2: sensor 1x1 + 2x1 (portrait cells)
w=place(sens,1,1,20,20)+place(sens,3,2,288,20)
fragment('gallery-sensor-sizes',w,[(20,150,'1x1 (default size)'),(288,150,'size: 2x1')],
 ['# left: 1x1, the default size','- entity: sensor.living_room_climate','  attribute: temperature','  details: [humidity]','',
  '# right: 2x1','- entity: sensor.living_room_climate','  name: Living room','  size: 2x1','  attribute: temperature','  details: [humidity, dew_point]','  decimals: 1'],836)
# gallery 3: climate wide
w=place(tab,6,1,20,20)
fragment('gallery-climate',w,[(20,150,'wide widget: - and + change the target')],
 ['- entity: climate.living_room','  name: Living room'],592)

# ---------- anatomy
ox,oy=40,30
W=1040
body='<rect id="bg" width="%d" height="%d" fill="#fff"/>\n'%(W,900)
# screen content: take everything between </defs> and </svg>
inner=re.search(r'</defs>(.*)</svg>',tab,re.S).group(1)
# strip invisible comments
inner=re.sub(r'<!--.*?-->','',inner,flags=re.S)
body+='<g id="screen" transform="translate(%d,%d)">%s</g>\n'%(ox,oy,inner)
body+='<rect id="screen-edge" x="%d" y="%d" width="960" height="540" fill="none" stroke="#888" stroke-width="2"/>\n'%(ox-1,oy-1)
def badge(n,cx,cy):
    return ('<circle id="bd-%d" cx="%d" cy="%d" r="14" fill="#fff" stroke="#000" stroke-width="3"/>\n'%(n,ox+cx,oy+cy)
        +txt('bdn-%d'%n,ox+cx,oy+cy+7,str(n),20,'#000','middle','font-weight="bold"'))
body+='<rect id="hl-slot" x="%d" y="%d" width="120" height="56" rx="8" fill="none" stroke="#000" stroke-width="3" stroke-dasharray="8 6"/>\n'%(ox+700,oy+4)
body+='<rect id="hl-free" x="%d" y="%d" width="552" height="104" rx="8" fill="none" stroke="#000" stroke-width="3" stroke-dasharray="8 6"/>\n'%(ox+392,oy+420)
for n,cx,cy in [(1,152,52),(2,430,12),(3,776,10),(4,834,32),(5,752,80),(6,436,182),(7,668,472)]:
    body+=badge(n,cx,cy)
ly0=oy+540+40
items=[(1,'Connection and last update. Tap to refresh now.'),
(2,'Tabs. Tap to switch; the filled tab is the open one.'),
(3,'Last tab slot. While asleep it shows the sleep pill:  '),
(4,'Battery level. Tap to open Settings.'),
(5,'Filled = on, outline = off (shape, not just grey).'),
(6,'Buttons on a widget: tap to act (shutter up, stop, down).'),
(7,'Free cells. The grid is 5 columns x 4 rows (landscape).')]
for k,(n,s) in enumerate(items):
    x=ox; y=ly0+k*38
    body+='<circle id="lg-c%d" cx="%d" cy="%d" r="14" fill="#fff" stroke="#000" stroke-width="3"/>\n'%(n,x+14,y)
    body+=txt('lg-n%d'%n,x+14,y+7,str(n),20,'#000','middle','font-weight="bold"')
    body+=txt('lg-t%d'%n,x+40,y+6,s,18,'#000')
# pill sample after item 3 (col 0,row 2): text ends ~x+40+370
px=ox+40+440; py=ly0+2*38-22
body+='<rect id="lg-pill" x="%d" y="%d" width="104" height="44" rx="12" fill="#000"/>\n'%(px,py)
body+='<use id="lg-pill-ic" xlink:href="#i-power" href="#i-power" x="%d" y="%d" width="20" height="20" color="#fff"/>\n'%(px+10,py+4)
body+=txt('lg-pill-m',px+36,py+14,'OFF',18,'#fff',extra='font-weight="bold" dominant-baseline="central"')
body+='<use id="lg-pill-ric" xlink:href="#i-refresh" href="#i-refresh" x="%d" y="%d" width="18" height="18" color="#fff"/>\n'%(px+11,py+23)
body+=txt('lg-pill-u',px+36,py+32,'15:30',18,'#fff',extra='dominant-baseline="central"')
body+=txt('lg-pill-cap',x:=ox,ly0+7*38+4,'Pill: OFF = Power off mode, 15:30 = next update. Dashed boxes show where the pill and free cells are.',16,'#444')
open(OUT+'screen-tab-anatomy.svg','w').write(HDR%(W,900,W,900)+'<defs>'+syms+'</defs>\n'+body+'</svg>\n')
