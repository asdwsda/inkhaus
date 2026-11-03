# Web flasher page (/install/) (DRAFT, in experiments)

Task 102. A page on the project site, not an e-ink screen: it follows site/style.css (not STYLE.md). White page,
black text, DejaVu Sans, 1 px #888 rules and card borders, 3 px black buttons with 4 px radius, #444 for secondary
text, #f2f2f2 for code blocks. No colour, no shadows, no animation. Wording reuses docs/ui/installer.md ("Your
settings and SD card are kept.", driver hint, data-cable hint). ESP Web Tools shows the connect/progress/done/error
dialog itself; this page does not redraw those states.

Mockups (docs/ui/experiments/, PNGs in experiments/png/):
`web-flasher-desktop.svg` (1200 wide, ready), `web-flasher-states.svg` (card states A-E), `web-flasher-narrow.svg`
(400 wide, unsupported browser, the phone case), `web-flasher-narrow-ready.svg` (400 wide, ready).

## Page structure (same header/footer/nav as index.html; Install is the current page)
1. Header: logo 48 + "Inkhaus" 28 bold, nav (Features, Install, Set up, GitHub) 18 underlined.
2. H1 "Install Inkhaus" (40, 32 under 600 px) and 3-line intro (18).
3. Install card (1 px #888 border, 24 px padding, 912 wide in a 960 wrap): states below.
4. "Before you start" (H2 28) with three `.steps` cards: data cable; Windows CH9102 driver (link; shown to all, text
   says "Mac and Linux do not"); right browser. Stack to one column under 900 px.
5. "After the install" (H2): copy `config.yaml` + `secrets.yaml` to the SD card root (FAT32), link docs/CONFIG.md,
   code block of the two files; narrow view adds "Firefox, Safari or offline? Use the installer apps."
6. Footer (16, #444).
Sections are separated by 1 px #888 rules, 64 px padding (48 narrow), as on the site.

## Install card
Elements (desktop; narrow = single column, select and buttons full width, buttons 56 high):
- "1. Choose a version" H3 20 bold.
- Version select: native `<select>`, 520x56, 3 px black border, 18 px text, filled triangle at right. Rows:
  "1.2.0 latest stable, 2026-11-01" (preselected = newest stable), then older stables newest first (last 10).
  Size ("2.3 MB", 16 #444) sits right of the select on desktop, hidden narrow (also in option text if wanted).
- Checkbox "Show pre-releases" 28 px box + 18 px label, whole row >= 48 px high. Off by default. On: pre-releases newer
  than the newest stable are added to the list, marked "PRE-RELEASE" (outline badge, 14 bold); selection stays.
- Downgrade note (only if the selected tag is older than the newest stable): #f2f2f2 box, 6 px black left bar,
  "This is older than the newest version (1.2.0). Settings saved by newer versions may be reset." Install button
  label becomes "Install 1.0.4 anyway". Pre-release selected: note "Pre-releases may have bugs." (same box).
- "2. Install" H3, then ESP Web Tools `esp-web-install-button` styled as `.btn.primary`, 320x56, label
  "Install <tag>" (slot="activate"). Its manifest attribute follows the select.
- Caption (16 #444): "A browser window asks for the USB port: pick the new entry that appears when you plug in
  the M5Paper. Your settings and SD card are kept." (No erase: `new_install_prompt_erase: false`.)

## States
A loading (`releases.json` pending): select dashed #888 "Loading versions...", Install dashed and disabled.
B picker open (native dropdown, drawn as a list; pre-release row with badge, selected row inverted).
C downgrade note (above).
D unsupported browser (no `navigator.serial`; detect by feature, not UA): picker, checkbox and Install are
replaced by "This browser cannot install from a page", "Firefox and Safari do not support Web Serial, which this page
needs to talk to the M5Paper. Open this page in Chrome, Edge or Opera on a computer, or use the installer app:"
and three 192x56 buttons Windows / macOS / Linux (narrow: stacked "Download for X"; the visitor's OS is primary/bold),
then "Offline or other ways to install: docs/FLASHER.md". Phones land here too.
E list failed (`releases.json` fetch error): "Could not load the version list", "Check your connection and reload the
page, or use the installer app.", Reload (primary) + Installer apps. Empty list (no releases) uses the same card.
Browser without JS: `<noscript>` shows state D text.
Staleness: the date next to each version is the release date; no auto-refresh.

## Interactions
- Tab order: nav, select, checkbox, Install, steps links. Enter/Space activate; 3 px outline focus ring (site style).
- Select change updates the Install label, size, downgrade note and the button's manifest. Checkbox change rebuilds
  the option list only. Install opens the ESP Web Tools dialog (its own UI); on finish the page is unchanged.
- Hover/focus on buttons inverts (site `.btn:hover`).

## Reflow
>= 900 px: as mockup. < 900: steps one column. < 600: H1 32, header column, buttons full width 56 high, select full
width, size line hidden, copy as in `web-flasher-narrow*.svg`.
