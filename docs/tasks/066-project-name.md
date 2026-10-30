# 066: Choose a new project name (Inkling may be taken)

Status: todo
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
- [ ] Findings with sources for Inkling and each proposal.
- [ ] User-approved name; no old name left (`grep -ri inkling` clean except history notes).
