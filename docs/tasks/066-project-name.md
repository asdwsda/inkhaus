# 066: Choose a new project name (Inkling may be taken)

Status: done
Depends on: none
UI spec: logo/splash follow-up by ui-designer once a name is chosen
Needs device: no

## Scope
1. Check "Inkling" for conflicts relevant to an open-source Home Assistant device project (GitHub repos, HA integrations /
   HACS, existing products and trademarks, domain names).
2. Propose ~5 alternatives with the same checks; the user picks.
3. Rename across the repo (README, CHANGELOG, docs, splash/logo text, icon data), then the logo/splash update.

## Files to touch
- Research note in this file; later README.md, CHANGELOG.md, docs/, src/render/splash_screen.h, icon data (via gen_icons.py)

## Acceptance criteria
- [x] Findings with sources for Inkling and each proposal.
- [x] User-approved name; no old name left (`grep -ri inkling` clean except history notes).
- Research (tester, 2026-10-30; full tables + sources in the session scratchpad 066-names.md): Inkling has no HA/HACS/ESPHome
  conflict but is crowded (489 GitHub repos, all package names and main domains taken, Wacom Inkling, a WIPO INKLING mark).
  Ranked alternatives: Inkhaus (cleanest), Tapink, Papertap, Paperdeck, Inkdeck (too close to an e-ink HA dashboard), Inkey.
- User (2026-10-30): new name **Inkhaus**.
- Device check 2026-10-30 (user, via the flasher test): Inkhaus splash shown; demo charts OK in both orientations.
