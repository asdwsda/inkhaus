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
    m=re.search(r'<g id="w-%d" transform="translate\([\d.]+,[\d.]+\)">'%n,src)
    i=m.end(); depth=1
    for t in re.finditer(r'<g[ >]|</g>',src[i:]):
        depth+=1 if t.group(0)!='</g>' else -1
        if depth==0: return src[i:i+t.start()]
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
    W=max(W,int(2*pad+32+12.1*max(len(l) for l in lines))+4)
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

# ====================================================================== batch 2
UI=R+'ui/'
_cache={}
def ld(n):
    if n not in _cache: _cache[n]=open(UI+n+'.svg').read()
    return _cache[n]
def all_syms(names):
    seen={}
    for n in names:
        for m in re.finditer(r'<symbol id="([^"]+)".*?</symbol>',ld(n),re.S):
            seen.setdefault(m.group(1),m.group(0))
    return ''.join(seen.values())
SYM2=all_syms(['tab-landscape','tab-landscape-asleep','status-states','settings-landscape','settings-sleep-landscape',
  'brightness-landscape','history-line-landscape','tab-portrait','sleep-screen-landscape','history-widget-states'])
def inner_of(n,keepbg=True):
    s=re.search(r'</defs>(.*)</svg>',ld(n),re.S).group(1)
    s=re.sub(r'<!--.*?-->','',s,flags=re.S)
    s=re.sub(r'<style.*?</style>','',s,flags=re.S)
    return s
def scaled(n,x,y,sc,prefix):
    b=inner_of(n)
    b=re.sub(r'id="',r'id="%s-'%prefix,b)   # unique ids, widget frames no longer lint targets
    b=b.replace('xlink:href="#%s-'%prefix,'xlink:href="#').replace('href="#%s-'%prefix,'href="#')
    return '<g transform="translate(%s,%s) scale(%s)">%s</g>\n'%(x,y,sc,b)
def write(name,W,H,body,syms=None):
    open(OUT+name+'.svg','w').write(HDR%(W,H,W,H)+'<defs>'+(syms or SYM2)+'</defs>\n<rect id="bg" width="%d" height="%d" fill="#fff"/>\n'%(W,H)+body+'</svg>\n')
def bdg(i,cx,cy):
    return ('<circle id="bd-%d" cx="%s" cy="%s" r="14" fill="#fff" stroke="#000" stroke-width="3"/>\n'%(i,cx,cy)
        +txt('bdn-%d'%i,cx,cy+7,str(i),20,'#000','middle','font-weight="bold"'))
def dashed(i,x,y,w,h):
    return '<rect id="hl-%s" x="%d" y="%d" width="%d" height="%d" rx="8" fill="none" stroke="#000" stroke-width="3" stroke-dasharray="8 6"/>\n'%(i,x,y,w,h)
def legend(x,y0,items,pitch=38):
    o=''
    for k,s in enumerate(items):
        n=k+1; y=y0+k*pitch
        o+='<circle id="lg-c%d" cx="%d" cy="%d" r="14" fill="#fff" stroke="#000" stroke-width="3"/>\n'%(n,x+14,y)
        o+=txt('lg-n%d'%n,x+14,y+7,str(n),20,'#000','middle','font-weight="bold"')
        o+=txt('lg-t%d'%n,x+40,y+6,s,18,'#000')
    return o
def shot(name,src,badges,items,dash=(),W=1040,note=None):
    ox,oy=40,30
    b='<g id="screen" transform="translate(%d,%d)">%s</g>\n'%(ox,oy,inner_of(src))
    b+='<rect id="screen-edge" x="%d" y="%d" width="960" height="540" fill="none" stroke="#888" stroke-width="2"/>\n'%(ox-1,oy-1)
    for k,d in enumerate(dash): b+=dashed(k,ox+d[0],oy+d[1],d[2],d[3])
    for i,cx,cy in badges: b+=bdg(i,ox+cx,oy+cy)
    ly=oy+540+40
    b+=legend(ox,ly,items)
    H=ly+len(items)*38+10
    if note: b+=txt('note',ox,H+6,note,16,'#444'); H+=30
    write(name,W,H,b)

# --- popup anatomy
shot('screen-popup-anatomy','brightness-landscape',
 [(1,336,184),(2,792,152),(3,284,262),(4,168,304),(5,792,304),(6,168,380),(7,634,380)],
 ['Name of the light.',
  'Close. A tap outside the panel, or PUSH, closes it too.',
  'Current brightness (number and bar). Changes at once.',
  'Minus and plus: 10 % per tap. UP / DOWN on the rocker do the same.',
  'Off: switches the light off.',
  'Presets: jump straight to 25, 50, 75 or 100 %.',
  'Filled preset = the value now. Closing sends the value.'])
# --- history popup
shot('screen-popup-history','history-line-landscape',
 [(1,306,108),(2,352,112),(3,630,134),(4,56,160),(5,500,300),(6,366,418),(7,904,100)],
 ['Name of the sensor.','Latest value.','Period and the lowest and highest value in it.',
  'Y axis: lowest and highest value of the graph.',
  'The line. The grey band is the min-max range of each slot.',
  'Dark grey bar under the line: a gap, no readings then.',
  'Close with X, a tap outside the panel or PUSH.'],
 note=None)

# --- settings
SETBACK='Back: one level up (the overview: leave and save).'
shot('settings-overview','settings-landscape',
 [(1,300,64),(2,810,26),(3,880,64),(4,478,330),(5,488,390),(6,488,466),(7,722,466)],
 ['Four categories. Tap one to open its page; the line shows its values.',
  'Unsaved: a value changed. It is saved when you leave Settings.',
  'Device info: firmware, battery, Home Assistant link, Wi-Fi.',
  'Config rows: the first error in config.yaml with its line.',
  'Sleep now: sleep at once (or hold PUSH 2.5 s).',
  'Refresh now: fetch all states again.',
  'Back: leave Settings and save. Long PUSH does the same.'])
shot('settings-display','settings-display-landscape',
 [(1,16,66),(2,100,110),(3,290,158),(4,824,26),(5,722,466)],
 ['Orientation row. Tap (or PUSH) to switch Portrait / Landscape.',
  'The dots are the choices; the filled dot is the current one.',
  'It turns when you leave Settings, not at once.',
  'Unsaved chip: shown after a change.',
  'Back to the overview.'])
shot('settings-sleep','settings-sleep-landscape',
 [(1,16,66),(2,16,166),(3,16,246),(4,16,326),(5,16,426),(6,488,66),(7,488,166),(8,488,330)],
 ['Sleep mode: Power off, Deep sleep, Touch, Touch & rocker.',
  'Updates while asleep: Off, 30 min, 1 h, 3 h, 6 h, 12 h.',
  'While asleep: show the Dashboard or the Sleep screen.',
  'Keep awake: Off, On USB or Always.',
  'Idle timeout: how long the screen stays awake after a touch.',
  'Learn battery limits: raises the low limit after brownouts.',
  'Note: appears when a choice has a catch (Touch wake).',
  'Battery use: 1 to 10 for the settings above; fewer is longer.'])
shot('settings-network','settings-network-landscape',
 [(1,16,66),(2,488,66),(3,488,166),(4,16,166),(5,722,466)],
 ['Wi-Fi: Auto tries your profiles; or force one profile.',
  'HA connection: Auto, Internal or External address.',
  'Report to HA: send battery and climate to Home Assistant.',
  'Note: a forced profile means other networks are not tried.',
  'Back to the overview.'])
shot('settings-tools','settings-tools-landscape',
 [(1,16,66),(2,488,66),(3,16,166),(4,722,466)],
 ['Demo mode: sample dashboard without Wi-Fi.',
  'Debug log: write log lines to the SD card (uses battery).',
  'Firmware: shows the version and whether an update exists.',
  'Back to the overview.'])

# --- status icons table: strips w-1..w-7 of status-states
ss=ld('status-states')
rows=[(1,'Connecting to Wi-Fi','one arc; label "connecting Wi-Fi"'),
(2,'Fetching states','cloud with an arrow; shown while the fetch runs'),
(3,'Online','full arcs; the last fetch is recent'),
(4,'Outdated (online)','clock after the time; old values stay visible'),
(5,'Offline','slashed arcs and clock; shows the last known state'),
(6,'Token rejected','padlock; create a new token'),
(7,'Demo mode','play badge; sample data, no network')]
W=1040; b=''
for k,(n,t1,t2) in enumerate(rows):
    y=20+k*80
    b+=place(ss,n,n,0+20,y)
    b+=txt('st-t%d'%n,600,y+22,t1,20,'#000',extra='font-weight="bold"')
    b+=txt('st-d%d'%n,600,y+46,t2,16,'#444')
# battery levels + sleep pill row
y=20+7*80+10
b+=txt('bl-h',20,y+14,'Battery icon',20,'#000',extra='font-weight="bold"')
for k,p in enumerate([100,78,50,25,10]):
    x=20+k*105; fw=int(32*p/100)
    b+='<g id="bt-%d"><rect x="%d" y="%d" width="36" height="20" fill="#000"/><rect x="%d" y="%d" width="32" height="16" fill="#fff"/><rect x="%d" y="%d" width="%d" height="16" fill="#000"/><rect x="%d" y="%d" width="4" height="10" fill="#000"/></g>\n'%(k,x,y+34,x+2,y+36,x+2,y+36,fw,x+36,y+39)
    b+=txt('bp-%d'%k,x+18,y+82,'%d%%'%p,18,'#000','middle')
b+=txt('bl-d',600,y+14,'Zz pill: asleep',20,'#000',extra='font-weight="bold"')
b+='<rect id="zz-pill" x="600" y="%d" width="96" height="44" rx="12" fill="#000"/>\n'%(y+30)
b+=txt('zz-t',648,y+61,'Zz',30,'#fff','middle','font-weight="bold"')
b+=txt('zz-d',714,y+58,'the screen is frozen; touch or press',16,'#444')
b+=txt('zz-d2',714,y+78,'the side button to wake it',16,'#444')
write('screen-status-icons',W,y+110,b,syms=all_syms(['status-states','tab-landscape']))

# --- orientation: portrait and landscape at 50 %
b=scaled('tab-portrait',40,60,0.5,'p')+scaled('tab-landscape',370,60,0.5,'l')
b+='<rect x="39" y="59" width="272" height="482" fill="none" stroke="#888" stroke-width="2"/><rect x="369" y="59" width="482" height="272" fill="none" stroke="#888" stroke-width="2"/>\n'
b+=txt('o1',40,36,'Portrait (540 x 960)',20,'#000',extra='font-weight="bold"')
b+=txt('o2',370,36,'Landscape (960 x 540)',20,'#000',extra='font-weight="bold"')
b+=txt('o3',370,380,'Settings > Display > Orientation switches between',18,'#000')
b+=txt('o4',370,406,'the two. The tab bar moves from the bottom to the top',18,'#000')
b+=txt('o5',370,432,'and the grid goes from 2 columns x 7 rows to',18,'#000')
b+=txt('o6',370,458,'5 columns x 4 rows. Widgets and tabs stay the same.',18,'#000')
write('screen-orientation',890,560,b)

# --- portrait columns 2/3/4: schematic grid cells from the real geometry
tp=ld('tab-portrait')
chrome=re.sub(r'<g id="w-\d+" transform.*?</g>\n','',inner_of('tab-portrait'),flags=re.S)
def cells(N):
    cw={2:248,3:161,4:118}[N]; gap=12 if N==2 else (12 if N==3 else 12)
    # widgets: (name,state,kind,on,wide)
    ws=[('Shutter','Opening',1,True),('Down shutters','Closed',1,False),('Ceiling','On',0,True),('Floor lamp','Off',0,False),
        ('Fan','Off',0,False),('Window','Open',0,False),('Living room','22.0',1,False),('Outside','8.4',0,False),('Night mode','On',0,True),('Home','12',2,False)]
    o=''; col=0; row=0
    def pos(c,r,span):
        return 16+c*(cw+gap), 68+r*116, span*cw+(span-1)*gap
    for k,(nm,st,kind,on) in enumerate(ws):
        span=(N if kind==1 else (min(2,N) if kind==2 else 1))
        if col+span>N: col=0; row+=1
        x,y,w=pos(col,row,span)
        fg='#fff' if on else '#000'
        o+='<g id="cl-%d"><rect x="%d" y="%d" width="%d" height="104" rx="8" fill="%s" stroke="#000" stroke-width="2"/>'%(k,x+1,y+1,w-2,'#000' if on else '#fff')
        fs=24 if N<4 else 18
        o+=txt('cn-%d'%k,x+12,y+(38 if N<4 else 30),nm if (N<4 or len(nm)<10) else nm[:7]+'...',fs,fg)
        if kind==1 and span>1:
            for j in range(3): o+='<rect x="%d" y="%d" width="62" height="62" rx="8" fill="#fff" stroke="#000" stroke-width="2"/>'%(x+w-86*(3-j)+22,y+21)
        o+=txt('cs-%d'%k,x+12,y+(77 if N<4 else 80),st,fs,fg)
        o+='</g>\n'
        col+=span
        if col>=N: col=0; row+=1
    return o
b=''
sc=0.4
for k,N in enumerate((2,3,4)):
    x=40+k*(540*sc+40)
    b+='<g transform="translate(%s,70) scale(%s)">%s%s</g>\n'%(x,sc,chrome,cells(N))
    b+='<rect x="%s" y="69" width="%s" height="%s" fill="none" stroke="#888" stroke-width="2"/>\n'%(x-1,540*sc+2,960*sc+2)
    b+=txt('cc-%d'%N,x+540*sc/2,52,'portrait_columns: %d'%N,18,'#000','middle',extra='font-family="DejaVu Sans Mono"')
    b+=txt('cd-%d'%N,x+540*sc/2,70+960*sc+28,{2:'default, cells 248 px',3:'cells 161 px',4:'cells 118 px'}[N],18,'#444','middle')
write('screen-columns-portrait',40*2+3*216+2*40,70+384+50,b)

# --- sleep screen variants
b=''
sc=0.45
land=[('sleep-screen-landscape','All blocks: weather, calendar, notifications'),('sleep-screen-landscape-none','No blocks listed: date, update, battery, indoor'),
      ('sleep-screen-landscape-fallback','A block has no data: it shows "-"'),('tab-landscape-asleep','While asleep = Dashboard: tab + Zz pill')]
for k,(n,lab) in enumerate(land):
    x=40+(k%2)*(432+40); y=50+(k//2)*(243+60)
    b+=scaled(n,x,y,sc,'s%d'%k)+'<rect x="%s" y="%s" width="434" height="245" fill="none" stroke="#888" stroke-width="2"/>\n'%(x-1,y-1)
    b+=txt('sl-%d'%k,x,y-10,lab,18,'#000')
yp=50+2*(243+60)+20
b+=txt('sp-h',40,yp-10,'Portrait (same four looks)',18,'#000',extra='font-weight="bold"')
port=['sleep-screen-portrait','sleep-screen-portrait-none','sleep-screen-portrait-fallback','tab-portrait-asleep']
for k,n in enumerate(port):
    x=40+k*(216+40)
    b+=scaled(n,x,yp,0.4,'q%d'%k)+'<rect x="%s" y="%s" width="218" height="386" fill="none" stroke="#888" stroke-width="2"/>\n'%(x-1,yp-1)
write('sleep-screen-variants',1064,yp+386+30,b)

# --- minimal config result
src=['version: 1','tabs:','  - title: Home','    widgets:','      - entity: light.living_room_ceiling','      - entity: sensor.outside_temperature','      - entity: cover.living_room_shutter']
b=''
sn,h=snippet(20,30,560,src)
b+=sn
b+='<rect x="620" y="29" width="482" height="272" fill="none" stroke="#888" stroke-width="2"/>\n'
b+=txt('mc-h',20,20,'config.yaml: 7 lines',18,'#444')
b+=txt('mc-h2',620,20,'the screen it makes',18,'#444')
# minimal screen: status bar + one tab + 3 widgets at 50 %
mini='<rect width="960" height="540" fill="#fff"/>'
tl=ld('tab-landscape')
hdr=re.sub(r'<g id="w-\d+" transform.*?</g>\n','',inner_of('tab-landscape'),flags=re.S)
def pl(n,newn,x,y): return place(tl,n,newn,x,y)
hdr=re.sub(r'<rect x="\d+" y="\d+" width="\d+" height="56" rx="8"[^>]*/>\s*<text[^>]*>[^<]*</text>','',hdr) if False else hdr
hdr=re.sub(r'<rect x="(305|437|569|701)" y="5"[^>]*/>','',hdr)
hdr=re.sub(r'<text x="(364|496|628|760)" y="[0-9]+"[^>]*>[^<]*</text>','',hdr)
hdr=hdr.replace('>Living</text>','>Home</text>').replace('y="21" font-size="18" fill="#fff" text-anchor="middle" dominant-baseline="central">Home','y="32" font-size="18" fill="#fff" text-anchor="middle" dominant-baseline="central">Home')
hdr=re.sub(r'<text x="232" y="43"[^>]*>room</text>','',hdr)
mini_body=hdr+pl(3,31,16,72)+pl(8,32,204,72)+pl(1,33,392,72)
b+='<g transform="translate(621,30) scale(0.5)">%s</g>\n'%re.sub(r'id="','id="mc-',mini_body)
b+=txt('mc-c1',620,330,'Three widgets, one tab. Names and states come',18,'#000')
b+=txt('mc-c2',620,356,'from Home Assistant. More widgets or tabs:',18,'#000')
b+=txt('mc-c3',620,382,'add more lines.',18,'#000')
write('install-minimal-config',1120,430,b)

# ====================================================================== batch 2: widget gallery (pre-103 types)
def wh(src,n):
    m=re.search(r'<g id="w-%d" transform="translate\([\d.]+,[\d.]+\)">\s*<rect id="w-%d-frame" x="-?[\d.]+" y="-?[\d.]+" width="([\d.]+)" height="([\d.]+)"'%(n,n),src)
    return int(float(m.group(1)))+2,int(float(m.group(2)))+2
def gal(name,items,lines,W,tag='config.yaml'):
    """items: (src, n, x, y, label) ; label under the widget; snippet below"""
    w='';labels=[];k=0
    for src,n,x,y,lab in items:
        k+=1; ww,hh=wh(src,n)
        w+=place(src,n,k,x,y)
        labels.append((x,y+hh+26,lab))
    fragment(name,w,labels,lines,W)
tlx=ld('tab-landscape'); tpx=ld('tab-portrait'); sdx=ld('sensor-details'); hwp=ld('history-widget-portrait'); hws=ld('history-widget-states')
# small widgets: switch, binary sensor, automation
gal('gallery-small-widgets',[(tlx,5,20,20,'switch (off)'),(tlx,7,208,20,'binary (open)'),(tlx,9,396,20,'auto. (on)')],
 ['- entity: switch.living_room_fan','  name: Fan','- entity: binary_sensor.living_room_window','  name: Window','- entity: automation.night_mode','  name: Night mode',
  '# switch and automation: tap toggles. binary_sensor: read only.'],592)
# cover and group
gal('gallery-cover-group',[(tlx,1,20,20,'cover: up is filled while it opens'),(tlx,2,20,170,'group: one button moves all members (up: waiting)')],
 ['groups:','  downstairs_shutters:','    - cover.living_room_shutter','    - cover.kitchen_shutter','    - cover.hall_shutter','tabs:','  - title: Living room','    widgets:',
  '      - entity: cover.living_room_shutter','        name: Shutter','      - group: downstairs_shutters','        name: Downstairs shutters   # name is required'],592)
# weather
gal('gallery-weather',[(tlx,10,20,20,'weather, 2x1 (default)')],['- entity: weather.home','  name: Home'],424)
# sensor states
gal('gallery-sensor-states',[(sdx,5,20,20,'unavailable'),(sdx,6,288,20,'loading'),(sdx,7,20,170,'2x1 unavailable')],
 ['# nothing to configure: the widget shows what Home Assistant says','- entity: sensor.living_room_climate','  name: Living room','  size: 2x1',
  '# dotted frame + n/a = unavailable; "-" = no value yet'],554)
# decimals before / after (text of the value only)
def decim(v):
    s=tlx
    g=grp(s,8).replace('8.4<tspan','%s<tspan'%v)
    return s.replace(grp(s,8),g)
d0,d1,d2=decim('8'),decim('8.4'),decim('8.40')
gal('gallery-sensor-decimals',[(d0,8,20,20,'decimals: 0'),(d1,8,208,20,'default (1)'),(d2,8,396,20,'decimals: 2')],
 ['- entity: sensor.outside_temperature','  name: Outside','  decimals: 0       # or 2; without it: 1 decimal','# unit: "°F"   replaces the unit text (max 8 characters)'],592)
# name override
def rename(txt_new):
    g=grp(tlx,4).replace('>Floor lamp<','>%s<'%txt_new)
    return tlx.replace(grp(tlx,4),g)
gal('gallery-name-override',[(rename('Light 1'),4,20,20,'no name'),(tlx,4,208,20,'with name')],
 ['# before: Home Assistant calls it "Light 1"','- entity: light.living_room_floor_lamp','','# after','- entity: light.living_room_floor_lamp','  name: Floor lamp'],520)
# graph types (display: graph, 2x2): line | step, bar
gx=20
items=[(hwp,1,20,20,'type: line (numbers, default)'),(hws,1,560,20,'type: step (on/off, default)'),(hws,2,20,300,'type: bar (statistics)')]
w='';labels=[]
for k,(src,n,x,y,lab) in enumerate(items):
    ww,hh=wh(src,n); w+=place(src,n,k+1,x,y); labels.append((x,y+hh+26,lab))
sn,hsn=snippet(560,300,480,['- entity: sensor.energy_today','  display: graph','  history:','    hours: 168','    type: bar','    statistic: sum','    period: day','# type: line or step for the others'])
labels2=labels
H=max(300+hh+26+30,300+hsn+20)
svg=HDR%(1100,H,1100,H)+'<defs>'+SYM2+'</defs>\n<rect width="1100" height="%d" fill="#fff"/>\n'%H+w
for i,(lx,ly,s_) in enumerate(labels2): svg+=txt('lb-%d'%i,lx,ly,s_,18,'#444')
svg+=sn+'</svg>\n'
open(OUT+'gallery-graph-types.svg','w').write(svg)

# ====================================================================== batch 3
# ---------- device-buttons: from the M5Stack drawing (size_s.jpg) and labelled photo (p01.jpg), 3.5 px per mm
def device_buttons():
    S=3.5; BW=int(67*S); BH=int(118.6*S)        # 234 x 415
    fx,fy=190,80                                # front body origin
    bx=745                                      # back body origin x
    bot=fy+BH
    o=''
    def line(i,x1,y1,x2,y2): return '<path id="%s" d="M%s %s L%s %s" stroke="#000" stroke-width="2" fill="none"/>\n'%(i,x1,y1,x2,y2)
    def dot(i,x,y): return '<circle id="%s" cx="%s" cy="%s" r="4" fill="#000"/>\n'%(i,x,y)
    o+=txt('t-front',fx+BW//2,40,'Front',24,'#000','middle','font-weight="bold"')+txt('t-front2',fx+BW//2,64,'(540 x 960 touch screen)',18,'#444','middle')
    o+=txt('t-back',bx+BW//2,40,'Back',24,'#000','middle','font-weight="bold"')
    o+=txt('t-back2',bx+BW//2,64,'(turned sideways)',18,'#444','middle')
    # front body, screen
    o+='<rect id="fr-body" x="%d" y="%d" width="%d" height="%d" rx="10" fill="#fff" stroke="#000" stroke-width="3"/>\n'%(fx,fy,BW,BH)
    sw,sh=int(58.32*S),int(103.68*S)
    sx=fx+(BW-sw)//2; sy=fy+int(7.5*S)
    o+='<rect id="fr-screen" x="%d" y="%d" width="%d" height="%d" fill="#fff" stroke="#000" stroke-width="2"/>\n'%(sx,sy,sw,sh)
    o+='<rect id="fr-tabbar" x="%d" y="%d" width="%d" height="22" fill="none" stroke="#000" stroke-width="2"/>\n'%(sx,sy,sw)
    o+='<rect x="%d" y="%d" width="44" height="12" rx="3" fill="#000"/><rect x="%d" y="%d" width="44" height="12" rx="3" fill="none" stroke="#000" stroke-width="2"/><rect x="%d" y="%d" width="44" height="12" rx="3" fill="none" stroke="#000" stroke-width="2"/>\n'%(sx+12,sy+5,sx+62,sy+5,sx+112,sy+5)
    for r in range(6):
        for c in range(2):
            fill='#000' if (r+c)%3==0 else '#fff'
            o+='<rect x="%d" y="%d" width="86" height="40" rx="5" fill="%s" stroke="#000" stroke-width="2"/>\n'%(sx+12+c*98,sy+36+r*52,fill)
    # rocker on the right edge: centre 58 mm from the bottom, 12.5 mm long
    rh=int(12.5*S); rc=bot-int(58*S); rt=rc-rh//2
    o+='<rect id="rk-body" x="%d" y="%d" width="9" height="%d" rx="3" fill="#000"/>\n'%(fx+BW-3,rt,rh)
    for k in (1,2):
        o+='<path d="M%d %d H%d" stroke="#fff" stroke-width="1"/>\n'%(fx+BW-2,rt+k*rh//3,fx+BW+5)
    # microSD on the right edge: 11 mm wide, 24.7 mm from the bottom
    sdh=int(11.1*S); sdc=bot-int(24.7*S)-sdh//2
    o+='<rect id="sd-slot" x="%d" y="%d" width="6" height="%d" fill="#555"/>\n'%(fx+BW-3,sdc-sdh//2,sdh)
    # bottom edge: USB-C (10.9 mm from the left), Grove A (right end 4.5 mm from the right edge, 12 mm)
    o+='<rect id="usb-port" x="%d" y="%d" width="34" height="8" rx="4" fill="#000"/>\n'%(fx+int(10.9*S)-17,bot-4)
    ax=fx+BW-int(4.5*S)-int(12*S)
    o+='<rect id="pa-port" x="%d" y="%d" width="%d" height="8" fill="#555"/>\n'%(ax,bot-4,int(12*S))
    # left edge: Grove B and C (12 mm each, 2 mm apart, lowest 11.4 mm above the bottom)
    for k,nm in enumerate(('pb-port','pc-port')):
        y1=bot-int((11.4+k*14)*S)-int(12*S)
        o+='<rect id="%s" x="%d" y="%d" width="8" height="%d" fill="#555"/>\n'%(nm,fx-4,y1,int(12*S))
    # callouts right of the front
    cx=450; ex=fx+BW+6
    o+=txt('rk-title',cx,84,'Rocker (right edge)',18,'#444')
    o+=txt('rk-up',cx,114,'UP',24,'#000',extra='font-weight="bold"')
    o+=txt('rk-up1',cx,138,'Previous tab or widget.',18)
    o+=txt('rk-up2',cx,160,'Yes in a yes/no question.',18)
    o+=txt('rk-push',cx,200,'PUSH',24,'#000',extra='font-weight="bold"')
    for k,s in enumerate(['Activate the focused widget.','Hold: open Settings.','Wakes from sleep.','Yes/no question: skip.']):
        o+=txt('rk-push%d'%k,cx,224+k*22,s,18)
    o+=txt('rk-down',cx,336,'DOWN',24,'#000',extra='font-weight="bold"')
    o+=txt('rk-down1',cx,360,'Next tab or widget.',18)
    o+=txt('rk-down2',cx,382,'No in a yes/no question.',18)
    o+=dot('rk-d1',ex,rt+rh//6)+line('rk-l1',ex,rt+rh//6,cx-8,108)
    o+=dot('rk-d2',ex,rc)+line('rk-l2',ex,rc,cx-8,194)
    o+=dot('rk-d3',ex,rt+5*rh//6)+line('rk-l3',ex,rt+5*rh//6,cx-8,330)
    o+=txt('sd-t1',cx,bot-46,'microSD slot',18,'#000')
    o+=txt('sd-t2',cx,bot-24,'(right edge, FAT32)',18,'#444')
    o+=dot('sd-d',ex,sdc)+line('sd-l',ex,sdc,cx-8,bot-52)
    # left callout
    o+=txt('gv-t1',20,bot-110,'Grove ports',18,'#000')
    o+=txt('gv-t2',20,bot-88,'B and C',18,'#000')
    o+=txt('gv-t3',20,bot-66,'(left edge)',18,'#444')
    o+=line('gv-l',135,bot-85,fx-4,bot-85)
    # bottom callouts
    ux=fx+int(10.9*S)
    o+=line('usb-l',ux,bot+4,ux,bot+34)+txt('usb-t1',ux,bot+56,'USB-C',18,'#000','middle')+txt('usb-t2',ux,bot+78,'charge + data',18,'#444','middle')
    px=ax+int(6*S)
    o+=line('pa-l',px,bot+4,px,bot+34)+txt('pa-t1',px,bot+56,'Grove port A',18,'#000','middle')+txt('pa-t2',px,bot+78,'(unused)',18,'#444','middle')
    # back
    o+='<rect id="bk-body" x="%d" y="%d" width="%d" height="%d" rx="10" fill="#fff" stroke="#000" stroke-width="3"/>\n'%(bx,fy,BW,BH)
    o+='<rect id="bk-label" x="%d" y="%d" width="%d" height="%d" rx="6" fill="none" stroke="#888" stroke-width="2" stroke-dasharray="8 6"/>\n'%(bx+30,fy+280,BW-60,110)
    o+=txt('bk-m5',bx+BW//2,fy+330,'M5Paper',18,'#888','middle')
    o+=txt('bk-mag',bx+BW//2,fy+354,'magnet',18,'#888','middle')
    rx_=bx+int(6.5*S); ry=bot-int(82.4*S)
    o+='<circle id="rst-hole" cx="%d" cy="%d" r="7" fill="#fff" stroke="#000" stroke-width="3"/>\n'%(rx_,ry)
    o+=line('rst-l',rx_,ry+7,rx_,ry+26)
    o+=txt('rst-t1',bx+14,ry+48,'RST / OFF (pinhole)',18,'#000',extra='font-weight="bold"')
    o+=txt('rst-t2',bx+14,ry+72,'on USB: reset',18,'#000')
    o+=txt('rst-t3',bx+14,ry+94,'on battery: switch off',18,'#000')
    o+=txt('rst-t4',bx+14,ry+116,'(use a paper clip)',18,'#444')
    o+=txt('foot',500,bot+112,'Positions from the M5Stack size drawing and product photo.',18,'#444','middle')
    write('device-buttons',1000,bot+130,o,syms='')
device_buttons()

# ---------- batch 3, part 2: state galleries drawn from the widgets.md rules (same geometry as the accepted mockups)
EXTRA=('<symbol id="i-dock" viewBox="0 0 24 24"><path d="M3 10 L12 3 L21 10 V21 H3 Z" fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round"/><path d="M12 8 V17 M8.5 13.5 L12 17 L15.5 13.5" fill="none" stroke="currentColor" stroke-width="2.2"/></symbol>'
 '<symbol id="i-dot-on" viewBox="0 0 24 24"><circle cx="12" cy="12" r="8" fill="currentColor"/></symbol>'
 '<symbol id="i-dot-off" viewBox="0 0 24 24"><circle cx="12" cy="12" r="8" fill="none" stroke="currentColor" stroke-width="2"/></symbol>'
 '<symbol id="i-door-on" viewBox="0 0 24 24"><rect x="4" y="3" width="16" height="18" fill="none" stroke="currentColor" stroke-width="2"/><polygon points="4,3 13,6 13,18 4,21" fill="currentColor"/></symbol>'
 '<symbol id="i-door-off" viewBox="0 0 24 24"><rect x="4" y="3" width="16" height="18" fill="none" stroke="currentColor" stroke-width="2"/><rect x="7" y="6" width="10" height="15" fill="none" stroke="currentColor" stroke-width="2"/><circle cx="15" cy="14" r="1.3" fill="currentColor"/></symbol>')
syms=SYM2+all_syms(['widgets-new-states-small','widgets-new-states-wide','widgets-new-media-art-states','widgets-new-popup-alarm'])+EXTRA
GRY='#888888'
def G(n,x,y,body): return '<g id="w-%d" transform="translate(%s,%s)">%s</g>\n'%(n,x,y,body)
def T(i,x,y,s,size=24,fill='#000',anc='start'):
    return '<text id="%s" x="%s" y="%s" font-size="%d" fill="%s" text-anchor="%s" dominant-baseline="alphabetic">%s</text>'%(i,x,y,size,fill,anc,s)
def U(i,ic,x,y,sz,col): return '<use id="%s" xlink:href="#%s" href="#%s" x="%s" y="%s" width="%d" height="%d" color="%s"/>'%(i,ic,ic,x,y,sz,sz,col)
def frame(n,W,mode):
    if mode in('on','pon'): s='fill="#000" stroke="#000" stroke-width="2"'
    elif mode=='un': s='fill="#fff" stroke="%s" stroke-width="2" stroke-dasharray="2 4"'%GRY
    elif mode=='poff': s='fill="#fff" stroke="#000" stroke-width="3" stroke-dasharray="8 6"'
    else: s='fill="#fff" stroke="#000" stroke-width="2"'
    r='<rect id="w-%d-frame" x="0" y="0" width="%d" height="104" rx="8" %s/>'%(n,W,s)
    if mode=='pon': r+='<rect x="5" y="5" width="%d" height="94" rx="5" fill="none" stroke="#fff" stroke-width="3" stroke-dasharray="8 6"/>'%(W-10)
    return r
def toggle(n,name,icon,state,mode,W=176,ic_off=None):
    """mode: on off pon poff un"""
    on=mode in('on','pon'); c='#fff' if on else '#000'
    if mode=='un': nc=GRY; ic='i-warn' if ic_off=='warn' else icon; icc=GRY
    else: nc=c; ic=icon; icc=c
    o=frame(n,W,mode)+T('w-%d-name'%n,12,38,name,24,nc)+U('w-%d-icon'%n,ic,12,49,40,icc)
    o+=T('w-%d-state'%n,58,77,state,24,'#000' if mode=='un' else c)
    if mode in('pon','poff'): o+=U('w-%d-badge'%n,'i-hourglass',W-36,57,24,c)
    if mode=='un' and ic_off!='warn': o+=U('w-%d-badge'%n,'i-warn',W-36,57,24,GRY)
    return G(n,0,0,o)
def pl(n,x,y,body): return G(n,x,y,re.search(r'<g id="w-\d+" transform="translate\(0,0\)">(.*)</g>',body,re.S).group(1))
def wide_btn(n,k,x,glyph,fill=False,pend=False,un=False,W=552):
    nm=['up','stop','down'][k] if isinstance(k,int) else k
    if un: r='<rect id="w-%d-%s-btn" x="%d" y="20" width="64" height="64" rx="8" fill="#fff" stroke="%s" stroke-width="2" stroke-dasharray="2 4"/>'%(n,nm,x,GRY); col=GRY
    elif pend: r='<rect id="w-%d-%s-btn" x="%s" y="21.5" width="61" height="61" rx="8" fill="#fff" stroke="#000" stroke-width="3" stroke-dasharray="8 6"/>'%(n,nm,x+1.5); glyph='i-hourglass'; col='#000'
    elif fill: r='<rect id="w-%d-%s-btn" x="%d" y="20" width="64" height="64" rx="8" fill="#000" stroke="#000" stroke-width="2"/>'%(n,nm,x); col='#fff'
    else: r='<rect id="w-%d-%s-btn" x="%d" y="21" width="62" height="62" rx="8" fill="#fff" stroke="#000" stroke-width="2"/>'%(n,nm,x+(0 if x==W-228 and False else 0)); col='#000'
    return r+U('w-%d-%s-icon'%(n,nm),glyph,x+12,32,40,col)
def cover(n,name,state,fills=(0,0,0),pend=None,un=False,W=552,glyphs=('i-up','i-stop','i-down'),names=('up','stop','down'),line2=None):
    o=frame(n,W,'un' if un else 'off').replace('width="%d"'%W,'width="%d"'%W)
    o+=T('w-%d-name'%n,12,44,name,24,GRY if un else '#000')
    o+=line2 if line2 else T('w-%d-state'%n,12,77,'n/a' if un else state,24,'#000')
    if un: o+=U('w-%d-badge'%n,'i-warn',W-264,57,24,'#000')
    for k in range(3):
        o+=wide_btn(n,names[k],W-228+76*k,glyphs[k],fill=bool(fills[k]),pend=(pend==k),un=un)
    return G(n,0,0,o)
def climate(n,name,mode='heat',target='22.0°',un=False,pend=None,W=552):
    o=frame(n,W,'un' if un else 'off')
    o+=T('w-%d-name'%n,12,44,name,24,GRY if un else '#000')
    if un: o+=T('w-%d-temp'%n,12,77,'n/a',24)
    else:
        o+=T('w-%d-temp'%n,12,77,'21.5°',24)+U('w-%d-hum-icon'%n,'i-drop',92,57,24,'#000')+T('w-%d-hum-text'%n,120,77,'45%',24)
        o+=U('w-%d-act-icon'%n,'i-flame' if mode=='heat' else 'i-power',186,57,24,'#000')
    if un: o+=U('w-%d-badge'%n,'i-warn',W-312,24,24,'#000')
    if pend is not None: o+=U('w-%d-badge'%n,'i-hourglass',W-312,24,24,'#000')
    for k,(nm,gl) in enumerate((('minus','i-minus'),('plus','i-plus'))):
        x=W-276 if k==0 else W-76
        o+=wide_btn(n,nm,x,gl,pend=(pend==k),un=un)
    o+=T('w-%d-target'%n,W-144,66,'n/a' if un else target,40,'#000','middle')
    return G(n,0,0,o)
def vacuum(n,name,status,batt,fills=(0,0,0),pend=None,un=False,err=False,low=False,W=552):
    o=frame(n,W,'un' if un else 'off')
    o+=T('w-%d-name'%n,12,44,name,24,GRY if un else '#000')
    st='n/a' if un else status
    o+=T('w-%d-state'%n,12,77,st,24)
    if not un and not err:
        wd=int(len(st)*13.4)+12+12
        o+=U('w-%d-bat'%n,'i-batt-low' if low else 'i-batt',wd,57,24,'#000')+T('w-%d-bat-text'%n,wd+28,77-6+6,batt,18)
    if un: o+=T('w-%d-bat-text'%n,150,77,'-',18)+U('w-%d-badge'%n,'i-warn',W-264,57,24,'#000')
    if err: o+=U('w-%d-badge'%n,'i-alert',W-264,57,24,'#000')
    for k,(nm,gl) in enumerate((('start','i-play'),('pause','i-pause'),('dock','i-dock'))):
        o+=wide_btn(n,nm,W-228+76*k,gl,fill=bool(fills[k]),pend=(pend==k),un=un)
    return G(n,0,0,o)
def sensor_txt(n,val,name,mode='n',W=176):
    o=frame(n,W,'un' if mode=='un' else 'off')
    o+=T('w-%d-value'%n,W//2,47 if mode!='un' else 53,val,24 if mode!='un' else 40,'#000','middle')+T('w-%d-name'%n,W//2,84,name,18,GRY if mode=='un' else '#444','middle')
    if mode=='un': o+=U('w-%d-badge'%n,'i-warn',W-36,12,24,GRY)
    return G(n,0,0,o)
# battery icons for the vacuum status line (S size, 24 grid): body + nub, fill 85 %
EXTRA2=('<symbol id="i-batt" viewBox="0 0 24 24"><rect x="2" y="7" width="18" height="10" fill="none" stroke="currentColor" stroke-width="2"/><rect x="4" y="9" width="13" height="6" fill="currentColor"/><rect x="20" y="10" width="2.5" height="4" fill="currentColor"/></symbol>'
 '<symbol id="i-batt-low" viewBox="0 0 24 24"><rect x="2" y="7" width="18" height="10" fill="none" stroke="currentColor" stroke-width="2"/><rect x="4" y="9" width="3" height="6" fill="currentColor"/><rect x="20" y="10" width="2.5" height="4" fill="currentColor"/></symbol>')
syms+=EXTRA2
def frag(name,items,lines,W,tw=None):
    """items: (builder-result body, x, y, label, label_dx, label_dy)"""
    w='';labels=[]
    for k,(b,x,y,lab,lx,ly) in enumerate(items):
        w+=pl(k+1,x,y,re.sub(r'w-\d+-','w-%d-'%(k+1),b).replace('id="w-%d"'%0,'')) 
        labels.append((x+lx,y+ly,lab))
    fragment(name,w,labels,lines,W)
