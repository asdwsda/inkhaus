# Website (one page, GitHub Pages)

Task 065. Static HTML/CSS, no JavaScript needed, no tracking, no web fonts from a CDN. No SVG mockup: the page
is plain flowing HTML; this spec is the reference.

## Style
- Looks like the device: white page (#FFF), text #000, secondary text #444, hairlines/dividers #888, quiet
  panels #CCC at most. No gradients, no shadows, no rounded corners over 8 px. Buttons: 3 px black border,
  white fill, black text; hover/focus = black fill, white text (filled = active, as on the device).
- Font: DejaVu Sans, stack `"DejaVu Sans", Verdana, system-ui, sans-serif` (self-host the DejaVu Sans woff2
  in site/ so it matches the device; no external requests). Body 18 px / 1.5, h1 40, h2 28, h3 20; bold for headings only.
- Screenshots (PNG from docs/ui/png) get a 2 px black border and a caption in #444 14 px; show at native size
  or scaled by CSS, never blurred (`image-rendering: auto`, give width/height attributes).
- Logo: docs/ui/logo-inkhaus.svg (copy to site/), 48 px in the header next to the word "Inkhaus" (28 px bold).
- Content width 960 px max, centred, 24 px side padding. Sections separated by a 1 px #888 line, 64 px vertical space.
- Contrast and focus: visible 3 px black focus outline; links underlined; respects `prefers-reduced-motion`
  (there is no motion anyway).

## Sections (top to bottom)
1. **Header**: logo + "Inkhaus"; right: links Features, Install, Set up, GitHub (wrap under logo on phone).
2. **Hero**: h1 "Your smart home on paper." Sub: "Inkhaus turns an M5Paper e-ink screen into a small remote
   for Home Assistant. Lights, blinds, heating and sensors on one calm screen that barely uses any battery."
   Buttons: [Download the installer] (primary, to #install), [See on GitHub].
   Image: tab-portrait.png and tab-landscape.png side by side (portrait left, landscape right).
3. **What it is** (3 short blocks, icon-free, numbered 1-3): "Tap to switch" (touch + the side rocker button
   also work), "Always readable" (e-ink, sunlight, no backlight glare, screen stays when asleep),
   "Weeks on a charge" (sleeps between touches, shows when it was last updated).
4. **Features** (2-column list, one line each): switches and lights, blinds and covers, thermostat,
   weather, vacuum, sensors with history graphs, brightness control, rotate portrait/landscape, settings on the
   device, your own tabs and layout in a text file, works on your home network without any cloud.
5. **Screenshots**: gallery with captions: sensor-details.png "Sensor details", history-line-portrait.png
   "History graph", brightness-portrait.png "Brightness", settings-portrait.png "Settings",
   tab-landscape-asleep.png "Asleep: still readable", splash-landscape.png "Start screen". Optionally flow.gif
   (docs/ui/flow.gif) as "Quick tour" under the gallery, max 960 px wide, with a text alternative.
6. **What you need** (checklist): an M5Paper; Home Assistant reachable on your Wi-Fi; a microSD card; a USB data
   cable (not charge-only); a Windows, macOS or Linux computer for the one-time install.
7. **Install** (id `install`): steps 1-3 in plain words:
   1. "Download the installer for your computer" - three buttons Windows / macOS / Linux (links to the latest
      release assets, `releases/latest/download/<asset>`; asset names from task 063). Under them: "Version x.y.z"
      filled in at build time from the latest release if available, else omitted.
   2. "Plug in the M5Paper and press Install. Keep the cable connected until it says done."
   3. "Your settings and SD card are not touched." Collapsible "Trouble?" (details/summary) with the
      FLASHER.md problems in one line each (data cable, Windows driver, SmartScreen/Gatekeeper warning
      because the app is unsigned, offline file option). Link to docs/FLASHER.md on GitHub for details.
8. **Set up** (id `setup`): "Put two small text files on the SD card": `secrets.yaml` (Wi-Fi name and password,
   your Home Assistant address and an access token; "keep it private, never share it") and `config.yaml` (which
   tabs and buttons you want). Show two short example blocks copied from secrets.example.yaml (placeholders only)
   and a minimal config.yaml example. How to create a token: 3 plain steps in Home Assistant (profile, Security,
   long-lived access token). Link to the repo docs for all widget types.
9. **FAQ** (details/summary, 4-5 items): Does it need the internet? How long does the battery last? Is it safe?
   (token stays on your SD card; only talks to your Home Assistant) Can I use it without the cloud? Which
   devices work? (M5Paper only).
10. **Footer**: GitHub link, licence, "Not affiliated with Home Assistant or M5Stack", "Made for e-ink".

Copy rules: short sentences, no jargon on first mention (say "access token (a password for apps)"), no
"firmware" without a hint ("the software on the device"), no emoji.

## Responsive rules
- Breakpoints: >= 900 px desktop; 600-899 tablet; < 600 phone. Single column HTML, so it reads in order everywhere.
- Desktop: hero image pair side by side; features 2 columns; gallery 3 columns; OS buttons in one row.
- Tablet: gallery 2 columns; hero images stay side by side, scaled.
- Phone: everything 1 column; hero images stacked and max 100% wide (portrait shot max 320 px, centred); gallery
  1 column; OS buttons full width stacked, each min 56 px high; header links wrap below the logo; code blocks
  scroll horizontally (`overflow-x:auto`), never shrink under 14 px.
- Touch targets >= 48 px high on phone with 12 px gaps. No horizontal page scroll at 320 px width.
- Images: `max-width:100%; height:auto`; `loading="lazy"` below the fold; always alt text.
- Page weight goal: under 1 MB without flow.gif; use PNGs as rendered, optionally quantised to 16 greys.
