# 065: Project website on GitHub Pages

Status: todo
Depends on: none (links to 063's downloads once they exist)
UI spec: ui-designer (one-page layout, reuses docs/ui mockups as images)
Needs device: no

## Scope
A simple one-page site built and deployed by GitHub Actions (Pages): what it is, mockup images, features, "Download the
flasher" links (latest release), how to prepare config.yaml/secrets.yaml, link to the repo. Static (plain HTML/CSS or a
minimal generator); no tracking.

## Files to touch
- site/ (or docs/site/), .github/workflows/pages.yml

## Acceptance criteria
- [ ] Builds locally; Pages deploy succeeds after the user pushes and enables Pages.
- [ ] Download links resolve to the latest release assets.
