# EPD research: M5Paper_FactoryTest (F) and giltal/ESP32_M5Paper (G)
Paths relative to scratchpad/ext/. F = factory/src, G = giltal/main/port. Clones read-only, nothing built.
NOTE: F links the M5EPD library (platformio.ini lib_deps), which is NOT in the clone. Driver internals
(VCOM, temperature, sleep register writes, alignment inside UpdateArea) are therefore from memory = UNVERIFIED for F.
G is a port of that driver (G/it8951.h:9-10 says "same protocol as M5EPD") and is verified.

## Modes (enum m5epd_update_mode_t = IT8951 waveform index: INIT0 DU1 GC16 2 GL16 3 GLR16 4 GLD16 5 DU4 6 A2 7, NONE8 = write only)
G/epd_display.h:18-26 enum; G/epd_display.cpp:27-41 maps to M5GFX only in fallback (DU4->epd_fastest, DU/A2->epd_fast, GL16*->epd_text, else quality).
Direct path (G/it8951.cpp:221-249, display_area) passes the raw mode number to the IT8951 -> all 8 waveforms.
Usage counts in F: GL16 33, NONE 33, GC16 15, DU4 12, A2 9, DU 1, INIT 1 (compare demo only).
- GC16: boot logo (F/systeminit.cpp:214), shutdown logo twice (F/global_setting.cpp:271-272), picture/txt viewer pages
  (frame_pictureviewer.cpp:77, frame_txtreader.cpp:107), every 5th frame switch (F/epdgui/epdgui.cpp:70-76), first frame.
- GL16: DEFAULT for all normal UI: frame switches (epdgui.cpp:75), status bar (frame_main.cpp:309), spinner/loading, info text,
  mutex switch redraw (epdgui_mutexswitch.cpp:67), keyboard full refresh (epdgui_keyboard.cpp:314,329,370), page number.
- DU4: default arg of every widget Draw (epdgui_button.h:26, base.h:28) = tap press AND release feedback, loading spinner,
  lifegame frames, shutdown prompt footer (frame_base.cpp:59,67).
- A2: textbox typing (epdgui_textbox.cpp:156,198), wifi list items (frame_wifiscan.cpp:158-196), home switch (frame_home.cpp:39),
  factory-test live values (frame_factorytest.cpp:225,258,379). 1-bit-like text/lines at low latency.
- INIT: Clear(true) at boot (systeminit.cpp:72) and on every frame exit (epdgui.cpp:~97, M5.EPD.Clear(true)). Only compare demo uses it as a mode.
- NONE: EPDGUI_Draw(UPDATE_MODE_NONE) (epdgui.cpp:67) composes all widgets into the controller memory, then ONE UpdateFull. 
  No per-widget flashes; this is the main reason a screen change looks clean.

## Full vs partial policy / ghost cleanup
- Frame switch: Clear(true)=INIT (full flash, ~1-2 s) on exit, then compose with NONE, then full GL16 (epdgui.cpp:66-76);
  GC16 on first frame or every >3 switches (frame_switch_count).
- Idle cleanup: after a touch, 2 s idle and M5.EPD.UpdateCount() > 4 partial updates -> ResetUpdateCount + UpdateFull(GL16)
  (epdgui.cpp:~115-122), switchable (_is_auto_update). This is the periodic ghost cleanup: cheap (GL16 full ~250 ms per G README),
  quiet, only after interaction, counter-based.
- Keyboard: full GL16 after key bursts (epdgui_keyboard.cpp).
- Shutdown: Clear() then logo GC16 x2 (global_setting.cpp:269-272), delay 600, power off EPD/EXT/main, esp_deep_sleep_start (no IT8951 standby call).
- No temperature handling in F app code (SHT30 is only a factory-test item, frame_factorytest.cpp:293). M5EPD driver: IT8951
  does its own temperature LUT selection (UNVERIFIED). VCOM: M5EPD begin sets it internally (UNVERIFIED); G leaves it to M5GFX (it8951.h:3).

## Canvas / framebuffer
- M5EPD_Canvas (TFT_eSPI-derived sprite) is 4bpp (16 grey, 0=white, 15=black) in the ESP's PSRAM. Per widget TWO canvases:
  _CanvasNormal + _CanvasPressed (epdgui_button.cpp:5-8, 29-35, 146-147: Pressed = copy + ReverseColor()). Pressed = inverted button
  (white on black), pushed with DU4 (default) or A2. Press and release each = one small-box DU4 push, ~100-150 ms.
- pushCanvas(x,y,mode) = WritePartGram4bpp (packed 4bpp straight into IT8951 memory, no RGB conversion, no dithering) + UpdateArea.
  Pixels are converted only in the IT8951 (4bpp->waveform). In G: 4bpp shadow buffer in PSRAM (epd_display.cpp:55-67), rows loaded
  with LOAD_IMG_AREA 4bpp big-endian (it8951.cpp:195-218), inverted with ~ because canvas 0=white but IT8951 0=black.
- Anti-aliased text drawn straight into 4bpp, so text has real greys; GL16/GC16 render them, DU4/A2 threshold them.
## Alignment
- G: x rounded UP to multiple of 4 (epd_display.cpp:~121,167,177: x=(x+3)&~3, w=(w+3)&~3), WritePart requires w%4==0 (~line 123-127);
  display_area widens areas lying within a single 4-px group (it8951.cpp:226-240) because the IT8951 can hang. Rotation mapped to native 960x540 coords.
- F: widgets are laid out on multiples of 4 (e.g. 92,182,356,300; 440-15); same rule as inherited driver.
## BUSY
- G: wait on HRDY before every SPI packet (WaitReady in load_area), per-row packets; refresh is fire-and-forget; waitRefresh() before the
  next load only if its rect overlaps the last refresh rect (epd_display.cpp:~215-233): non-overlapping updates run in parallel on controller;
  LUT-AFSR register 0x1224 polled (busy(), it8951.cpp:251), 30 ms minimum guard after a display command (busy reads idle too early), 5 s timeout.
- F: UpdateArea waits for the previous refresh (CheckAFSR) before loading (M5EPD, UNVERIFIED); CheckAFSR() called explicitly to time (frame_compare.cpp:54).
## ESP-IDF port (G) changes
- Replaces M5EPD with M5Unified/M5GFX for boot/touch + own it8951 SPI driver; modes mapped directly (all 8 waveforms); full-screen update ~160 ms vs ~250 ms
  (G README); mutex around everything (SPI bus shared with SD); RGB palette hack only for the fallback path; SD fixes. No sleep/standby or temperature code either.

## Why the factory app looks clean (synthesis)
1. Never uses DU/DU4/A2 for content changes that must stay; they only draw press feedback and are followed by GL16 full after 2 s idle if >4 updates.
2. Compose offscreen (NONE) then one full-screen GL16 (or GC16 every 5th): no partial-mode residue, no per-widget flicker.
3. INIT waveform full clear on every screen exit wipes accumulated grey drift.
4. Mostly black text on white, 4bpp raw data (no dither), GL16 has low ghosting for white-background text.
5. Pressed state is a pre-rendered inverted canvas: single DU4 push, no layout computation.

## Our approach (relevant lines)
docs/ARCHITECTURE.md:147-152 (refresh), 254-261 (decisions), 283 (grey drift). src/present.cpp:156-165 refresh(), 296-316 refreshRect (fastest vs quality),
268-281 fullRefresh (GC16 white fill + GC16), 25 kQuietMs idle ghost-debt full (664 idleFull), 168-180 pushRegion (B/W threshold). Only epd_quality / epd_fastest
are used for content; epd_fast and epd_text measured bad (skill table). src/boot.cpp:306 controllerStandby; src/sleep.cpp:276-280 standby before deep sleep.
No INIT waveform (ARCHITECTURE.md:257), no GL16 full, no A2, no direct IT8951 access beyond M5GFX.

## Ideas (ranked by visual benefit / cost)
1. Idle cleanup with a cheaper, calmer full: after quiet 2 s and N partials, full GL16 instead of GC16 white-fill + GC16 (F epdgui.cpp:~115-122;
   ours present.cpp:664 idleFull, :268 fullRefresh). Needs GL16: M5GFX epd_text. CAVEAT: our measurements say epd_text ghosts heavily on partial boxes (910 ms);
   F uses GL16 only FULL-SCREEN over a mostly-GC16 screen. Test GL16 FULL only. Low cost (one setEpdMode), medium benefit (shorter flash, less drift?
   but ARCH:283 says every partial darkens untouched pixels; GL16 may not fix that).
2. INIT waveform for the full clear on page change / before sleep (F systeminit.cpp:72, epdgui.cpp:~97; G it8951.cpp:221 mode 0). INIT is the real
   anti-ghost/anti-drift clear (long ~1-2 s, strong flash). Needs raw display command (G it8951.cpp:221-249 display_area; M5GFX doesn't expose; ARCH:257).
   Benefit: possibly fixes grey drift (ARCH:283) better than GC16 white fill; cost: new low-level path, flash visible, power ~1-2 s per use. Use rarely (daily/page change on user request).
3. Compose-then-one-update on screen change: F draws all widgets with NONE and issues one UpdateFull (epdgui.cpp:66-76). We already render a frame buffer
   and diff (present.cpp), so already equivalent; check only that tab change does not issue several refreshes (grep "PRESENT: refresh" in log).
4. Direct 4bpp load (G it8951.cpp:195-218, epd_display.cpp:55-67 with ~invert): avoids M5GFX RGB conversion/dither (our colour bug ARCH/skill "Colours").
   Benefit: grey text exact, no dither, faster loads; cost: high (bypasses M5GFX, shared SPI with SD, per-row HRDY). Only if greys/quality are targeted.
5. Press feedback as pre-rendered inverted box with A2/DU4 (F epdgui_button.cpp:146-147, G A2->DU4). We already use fastest (=DU4) thresholded; adopting A2
   (G it8951.cpp mode 7) is faster, but ghosts more; low priority.
6. Overlap-aware parallel refresh + 30 ms start guard (G epd_display.cpp:~215-233): we wait via waitDisplay; may allow overlapping non-intersecting fastest
   rects to overlap in time. Cost low-medium, benefit only latency on multi-widget updates.
7. 4-pixel alignment: ours rounds? check src/present.cpp rect align; G widens single-4px-group updates (it8951.cpp:226-240) because IT8951 hangs;
   M5GFX has the same guard (G comment). Nothing to do unless we see hangs.
8. Shutdown: F does no IT8951 standby; we do (boot.cpp:306) - ours is better. No change.
No VCOM/temperature work exists in either; nothing to adopt.
