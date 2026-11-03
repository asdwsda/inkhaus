# 110: User guide on the website (structure and build)

Status: todo
Depends on: 065, 102
UI spec: none (site style; page layout by ui-designer in 111)
Needs device: no

## Scope
A user guide written in Markdown under `docs/guide/` (single source), rendered by `site/build.py` into `site/dist/guide/`
pages with a sidebar/TOC, linked from the home page and the install page. The repo's docs/CONFIG.md becomes a short pointer
to the guide's config reference (no duplicate content).
- Pages (first cut): Getting started, The screen (anatomy), Touch & buttons, Widgets (gallery, one section per widget type),
  Tabs & layout, Settings (one section per page), Sleep & battery, Sleep screen, Config reference (every key), Troubleshooting.
- Markdown -> HTML with a small pinned dependency or Python's stdlib-only renderer (decide; no Node toolchain), heading
  anchors, code blocks with a copy button, image figures with captions, light/dark like the site.
- `site/check.py`: page weight per page (not the whole site) with the same 1 MiB cap; every image has alt text;
  every config key in the reference appears in config.example.yaml and vice versa (lint), broken anchors fail.
- Out of scope: search, translations (user: not needed), the images themselves (task 111).

## Files to touch
- docs/guide/, site/, tools/ (110, 111); docs/ui/ (111, ui-designer)

## Acceptance criteria
- [ ] `python3 site/build.py && python3 site/check.py` pass; guide pages exist with nav and anchors.
- [ ] Config-key lint passes against config.example.yaml and lib/config (key list extracted, not hand-kept).
- [ ] Pages workflow deploys the guide (site path filter includes docs/guide/**).

## Notes
- User 2026-11-03: docs must be as user friendly as possible, part of the website, with widget screenshots and options.
