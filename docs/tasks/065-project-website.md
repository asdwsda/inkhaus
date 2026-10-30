# 065: Project website on GitHub Pages

Status: done
Depends on: none (links to 063's downloads once they exist)
UI spec: docs/ui/website.md
Needs device: no

## Scope
A simple one-page site built and deployed by GitHub Actions (Pages): what it is, mockup images, features, "Download the
flasher" links (latest release), how to prepare config.yaml/secrets.yaml, link to the repo. Static (plain HTML/CSS or a
minimal generator); no tracking.

## Files to touch
- site/ (or docs/site/), .github/workflows/pages.yml

## Acceptance criteria
- [x] Builds locally; Pages deploy succeeds after the user pushes and enables Pages.
- [x] Download links resolve to the latest release assets.
- GitHub check 2026-10-30: v1.1.0 release run green (firmware, 3 installers, manifest); site live at https://asdwsda.github.io/inkhaus/, download links resolve.
