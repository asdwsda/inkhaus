# 073: Showcase the history graph in the README and on the website

Status: todo
Depends on: 071, 070, 065
UI spec: docs/ui/history-widget.md, docs/ui/website.md
Needs device: no

## Scope
User (2026-10-30): once the graph widget (071) is done, update the README and the GitHub Pages site with
images that show history graphs, and make them as striking as possible.
- ui-designer: a new hero mockup set (portrait and landscape) whose first tab features a graph widget
  plus the history popup; optionally a composite "device on a desk" style frame and a short animated
  sequence (wake, tab with graph, popup) if it stays light (< 1 MB total added).
- README.md: hero images replaced, a "History graphs" line in the feature list with an image.
- site/: hero and features section updated; site/build.py and site/check.py pass.
- Images come from rendered mockups (no device photos needed).
Out of scope: new firmware features.

## Files to touch
- docs/ui/ (new mockups + PNGs), README.md, site/.

## Acceptance criteria
- [ ] New mockups rendered and checked (tools/render_mockups.sh, tools/check_mockups.py).
- [ ] README shows a graph widget and the history popup; `python3 tools/check_links.py` prints "links ok".
- [ ] site/build.py and site/check.py pass; total added image weight < 1 MB.
- [ ] User approves the look.

## Notes
