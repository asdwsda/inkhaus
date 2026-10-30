# Inkhaus Installer window (DRAFT, variant pending user choice)

Task 069. Desktop window (Rust egui/eframe), not e-ink. Content area 560x420 logical px (egui points; scales with the
OS DPI). Minimum size 520x400, resizable but the layout is a centred 560-wide column. Flow and wording stay as in
docs/FLASHER.md; no new features. Mockups: `docs/ui/installer-a.svg`, `-b.svg`, `-c.svg` (light, all 6 states each),
`installer-dark.svg` (dark theme sample). PNGs in docs/ui/png/.

## States (same in every variant)
1. No device: "Plug in your M5Paper", hint "Use a USB cable that carries data, not only power.", driver link
   (shown on Windows, as today), Install disabled. If several candidates: the existing port ComboBox appears in the card.
2. Ready: "M5Paper connected", port, version dropdown (newest stable preselected), "Show pre-releases",
   "Choose file..." (small link next to the dropdown, offline use), note "Your settings and SD card are kept.",
   Install (primary), "Upload YAML..." (secondary).
3. Installing: phase text (Downloading / Checking files / Writing / Verifying / Restarting) + percent, progress bar,
   "Do not unplug the M5Paper." Buttons disabled ("Please wait"). Indeterminate phases: bar shows a slow moving block, no percent.
4. Done: "All done", "Version X is installed and the M5Paper is restarting. Your settings were kept." Primary becomes "Install again".
5. Error: red title "Something went wrong", plain sentence from `e.message()`, collapsible "Show details" (raw text,
   copyable), primary "Try again". Checksum/rate-limit hints from FLASHER.md Problems may appear as the sentence.
6. YAML upload: title "Upload YAML files", file list (secrets.yaml, config.yaml) with sent/sending marks, progress,
   "Do not unplug". On success the same Done card ("Uploaded. The M5Paper restarts with the new files."). File contents are never shown.
Step labels (A): Connect, Version, Install. Done keeps all three checked; error marks Install red.

## Variants
- **A. Single-card stepper** (safest, most familiar): header with logo, 3-step progress row, one grey card with the
  state content and a status glyph (green check / red X / dashed USB ring), full-width 56 px button; Upload YAML is a
  outlined button beside Install when ready. Colour accents: green and red only for state.
- **B. Device-centric** (friendliest, most work): left 208 px panel with an M5Paper illustration whose screen mirrors the
  state (dim logo = no device, logo = ready, logo + bar = working, check, X) and a cable (dashed = unplugged,
  solid = connected). Right column has the status dot, text and 56 px button; "Upload YAML files..." is a link above it.
  Illustration is drawn with egui painter shapes (rounded rects, lines); no image asset needed.
- **C. Minimal e-ink** (matches device and website): pure black on white, 3 px rules, square status marks
  (filled / outline / crossed) instead of colour, 24-block progress bar, buttons with 3 px black border, no rounded
  corners over 0-2 px, disabled = dashed outline. Zero colour: error is shown by the crossed mark and the word ERROR.

## Shared tokens
| token | light | dark |
|---|---|---|
| bg | #FFFFFF | #1A1A1A |
| panel | #F3F3F3 | #262626 |
| ink / text | #111111 | #F2F2F2 |
| secondary text | #555555 | #B0B0B0 |
| hairline | #BBBBBB | #555555 |
| primary button fill / text | #111 / #FFF | #F2F2F2 / #111 |
| disabled fill / text | #E4E4E4 / #8A8A8A | #3A3A3A / #888888 |
| ok (A,B only) | #1B7F3B | #5FCB82 |
| error (A,B only) | #B3261E | #FF8A80 |
Theme follows the OS (`ctx.style().visuals` dark/light); logo is drawn black on light, white on dark with the knob in bg colour.

## Type, spacing, sizes
- Font: DejaVu Sans Regular + Bold (as device and website), embedded with `include_bytes!` and set as Proportional
  (about 750 KB for both; subset to Latin if size matters). Sizes in pt: title 28 (C) / 22 (A,B), body 16, secondary 14, button 22 bold, header 20 bold.
- Margins 24 px (28 in C); gap 12-16 px; buttons 56 px high (60 in C), corner radius 8 (A,B), 0 (C).
- Only egui primitives: rect/circle/line/polyline painter calls, `ProgressBar` or painted rects, `ComboBox`, `Checkbox`.
  No gradients, shadows, blur or animation (progress repaints at 10 Hz only while working).

## Assets to embed
- Logo: render `logo-inkhaus.svg` to PNG (128x128, black; draw it tinted for dark via `Image::tint`) or paint the
  path with `egui::Shape::line` (two polylines + rounded rect + circle, coordinates in the SVG). PNG 128 px ~2 KB. Window/app icon: same logo on white at 256 px.
- Fonts: DejaVuSans.ttf, DejaVuSans-Bold.ttf.
- Illustration (B): painter shapes only.

## Interactions
Mouse/touch and keyboard: Tab order version dropdown, Install, Upload YAML; Enter = primary button, Esc = close
"Show details". Buttons have a visible 3 px focus ring. While working only "Show details" stays interactive. Hit targets >= 44 px high.

## Recommendation (designer)
A for lowest risk and best explanation of progress; C if the look should match the device/website exactly; B is the
friendliest for non-technical users but costs the most code. Dark sample shown for A; B and C use the same token swap.
