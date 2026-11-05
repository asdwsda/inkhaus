# Device config editor on /install/ (DRAFT, in experiments)

Task 119. A section on the install page (docs/ui/web-flasher.md, site/install.html), not an e-ink screen: it follows
site/style.css (white page, black text, DejaVu Sans, 1 px #888 card borders, 3 px black buttons with 4 px radius, #444
secondary text, #f2f2f2 note boxes with a 6 px black left bar, no colour, no animation). State is shown by shape
(filled = primary or error, dashed = disabled or idle), not by colour. Talks to the M5Paper with the provisioning
protocol over Web Serial (lib/provision, tools/provision.py). The protocol has `putfile` only; a read command is a
firmware dependency (task 119 "Files to touch"). Without Web Serial the section is hidden and the existing
"This browser cannot install from a page" card stays.

Mockups (docs/ui/experiments/png/): `config-editor-desktop.svg` (1200 wide; read, edited, 1 error + 1 warning, secrets
collapsed), `config-editor-secrets.svg` (secrets step open, one invalid field), `config-editor-states.svg` (flow states A-F).
Narrow (< 600 px) reflows as the flasher: single column, buttons full width 56 high, editor full width, field pairs stacked.

## Page placement
New H2 "Edit device config" below the install card (or its own link in the nav-less page, anchor `#config`), same wrap
(960), card 912 wide, 24 px padding, cards 32 px apart. Intro (18): "Read config.yaml from the M5Paper, change it here and
write it back over USB. Everything stays in this browser. Nothing is uploaded." Needs the device running the same
Inkhaus version that supports `getfile` (else error D2).

## Config card (desktop mockup coordinates, card x144 y290 w912)
Inner x 168..1032 (864 wide). Heights below are px from the card top.
1. "1. Connect and read" H3 20 bold (y+50). Button "Read config from device" primary 320x56 at (168, y+66); right of it
   caption 16 #444 "Reads config.yaml only. Secrets are never read." Status strip 864x56 at y+138: 1 px black border,
   4 px radius; glyph 24 at x+28 (check = done, half circle = busy, triangle = error), head 18 bold, detail 16 #444.
2. "2. Edit config.yaml" H3 (y+244). Editor 864x304 at y+262: 3 px black border, 4 px radius, gutter 56 wide #f2f2f2
   with line numbers (14 mono, #888), text 15 DejaVu Sans Mono, line height 24, scrolls vertically (resize handle off).
   Marks: error line = black gutter cell with white number, row #f2f2f2 with 6 px black left bar; warning line = outlined
   number box and a 2 px #888 underline. Disabled/empty state: dashed #888 border, text "Read the device first."
3. Check (y+598): "Check: 1 error, 1 warning" 18 bold ("Check: OK" with check glyph when clean). One row per message,
   48 px pitch: 28 px marker (filled square "!" = error, outline square "?" = warning), "Line N" 18 bold at x+44, text
   16 at x+134. Updates 300 ms after the last keystroke. Max 8 rows, then "and N more". Click a row: caret jumps to the line.
4. "3. Write to device" H3. Buttons 56 high: "Write to device" primary 320 wide (dashed #888 and disabled while any
   error, while not read, or while busy); "Discard edits" secondary 240 wide (restores the text read from the device).
   Caption 16 #444: "Disabled while the check shows an error. The M5Paper restarts and loads the new layout (about 10 s). Limit 64 KB."
   Warnings do not block writing.

## Secrets card (optional, separate card, collapsed by default)
Collapsed (desktop mockup): H3 "Optional: Wi-Fi and Home Assistant token", caption "Only needed on a new device or
when they change. Nothing you type is stored or sent.", secondary button "Set up secrets..." 320x56. Works without
step 1 (needs only a connection).
Open (`config-editor-secrets.svg`):
- Notice box (864x150, #f2f2f2, 6 px black bar), text 16, heading 18 bold, wording fixed:
  "Your secrets stay in this browser" /
  "This page does not store, send or log what you type here." /
  "It goes from this browser to the M5Paper over the USB cable, nowhere else." /
  "Write-only: the device never sends secrets back, so these fields start empty." /
  "Writing replaces the whole secrets.yaml on the device, all Wi-Fi networks included."
- Fields, two columns of 420x56 (3 px black border, 4 px radius, 18 px text, label 16 bold above): Wi-Fi name (SSID),
  Wi-Fi password (masked), Home Assistant address, Access token (masked). Rows 120 px apart. Message line under a
  field: marker 22 px + 16 px text. Invalid field: 6 px black bar inside the left edge + "!" message.
  Checkbox "Show characters" (28 px box, 18 label, row >= 48 high) unmasks the two masked fields; reset on write/clear.
- Caption: "More Wi-Fi networks, a fixed IP or an outside address: write secrets.yaml as a file (see docs/CONFIG.md), or
  paste a whole file instead." ("paste a whole file" swaps the four fields for one text area, same rules, same notice.)
- Buttons: "Write secrets to device" primary 320 (dashed disabled until required fields valid), "Clear fields" secondary 240.
- Fields use `autocomplete="off"`, no `name` attributes that a password manager would save, never read back, never in
  localStorage/sessionStorage/URL/console. They are cleared after a successful write, on "Clear fields", on pagehide,
  and when the connection closes.

## Validation messages (config.yaml; checked in the browser, same rules as the device parser and site/configkeys.py)
Errors (block Write):
- "YAML syntax error: <reason>." (unclosed quote, bad indent, tab character, duplicate key)
- "Missing ':' after '<key>'." Hint: "Write it as '<key>: value'."
- "'version' is missing. Add 'version: 1' at the top."
- "No tabs. Add at least one tab with widgets."
- "Too many tabs (max 16)." / "Too many widgets in this tab (max 24)."
- "Not an entity id: '<text>'. Use domain.name, for example light.kitchen."
- "Unknown domain '<domain>'. Supported: cover, light, switch, ..."
- "Group '<name>' mixes domains." / "Group name '<name>': lowercase letters, digits and underscore only."
- "Group '<name>' is not defined." / "File is over 64 KB."
Warnings (do not block):
- "Unknown key '<key>'. The device ignores it." (with "Did you mean '<x>'?" when the edit distance is <= 2)
- "Two widgets use '<entity>'." / "Tab '<title>' has no widgets."
Secrets fields (checked as typed, shown after the first blur or on Write click):
- "Required." (SSID, address, token) / "Must start with http:// or https://" (address) /
  "The outside address must use https" (only in pasted-file mode) / "Looks short. Tokens have over 100 characters." (warning)
- Password: "Wi-Fi password must be 8 to 63 characters, or empty for an open network." (error if 1-7).

## Flow states (`config-editor-states.svg`)
A Not connected: Read primary, Write dashed, editor dashed, secrets collapsed. Nothing runs until a click (user gesture).
B Connecting: Read becomes dashed "Connecting...", strip busy "Waiting for the USB port" / "Pick the M5Paper in the browser
  window." Cancelling the port dialog goes back to A silently.
C Reading: strip busy "Reading config.yaml", "Do not unplug the cable."; progress bar 864x24 (3 px border, black fill),
  caption "N of M bytes". 3 s without a byte aborts to D.
Ready (desktop mockup): check strip "Read config.yaml, 1,204 bytes" / "From the M5Paper (Inkhaus 1.5.0) at 14:32. Your
  edits stay on this page until you write." The time is the read time (staleness).
D Error (strip with 3 px border, #f2f2f2, triangle; Try again primary; device and page edits unchanged). Messages:
  1 "No port chosen or port busy": "Close serial monitors and the installer app, then try again."
  2 "The M5Paper did not answer": "Press the rocker once to wake it. It needs Inkhaus 1.5 or newer."
  3 "Cable unplugged during the transfer": "Plug it in and try again. The old file on the device is intact."
  4 "The device refused the file": "Checksum or size (over 64 KB) was wrong. Try again."
E Writing: all buttons dashed, strip busy "Writing config.yaml", progress bar, "N of M bytes, checking on the device".
F Written: strip with check "config.yaml written and checked" / "The M5Paper restarts now (about 10 s)."; Read again
  secondary. Editor keeps its text. For secrets the strip reads "secrets.yaml written" / "The fields were cleared." and
  the fields are empty. While the device restarts, Read and Write are disabled for 10 s.
Beforeunload: leaving with unwritten edits asks the browser's confirm; with typed secrets no text is shown anywhere.

## Interactions
Tab order: Read, editor, check rows, Write, Discard, "Set up secrets...", secrets fields, Show characters, Write
secrets, Clear. Enter/Space activate; 3 px focus ring (site style). Hover/focus inverts buttons (site `.btn`). Editor
Tab inserts nothing (Tab leaves the field; Esc not needed). Redraw: only the strip, check list and button states change;
the editor text is never replaced after a successful read except by Read or Discard.
