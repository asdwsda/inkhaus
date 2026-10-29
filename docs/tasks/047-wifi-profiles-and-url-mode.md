# 047: Wi-Fi profiles and HA URL mode

Status: todo
Depends on: 021, 032, 026
UI spec: docs/ui/settings.md (new "Network" page or rows) — ui-designer
Needs device: yes (later, home Wi-Fi + hotspot)

## Problem
Today: remembered base URL first, else internal then external; switch only on a network error. Once the
external URL was used (hotspot), the device stays on it at home too (it keeps working, just slower) — no
path back to internal.

## Design (user idea + architect proposal, 2026-10-30)
- secrets.yaml: `wifi:` becomes a list of profiles `{name, ssid, password, ha_url: internal|external|auto,
  optional static_ip}`; a single mapping (today's format) is still accepted as a list of one.
- Settings (new): **Wi-Fi**: Auto / <profile names> (Auto = first known network in range, list order; a
  name forces it). **HA connection**: Auto / Internal / External (Auto = the connected profile's `ha_url`;
  for `auto` profiles the remembered base is stored per SSID, so coming home returns to internal).
- The Wi-Fi fast-reconnect cache becomes per profile.
- Never log SSIDs beyond length + 2 chars, never passwords/URLs.
- Settings show the connected profile name and the link in use.

## Steps
1. ui-designer: where the two settings live (a "Network" page like Tools, or rows), mockups.
2. firmware-dev: lib/config (secrets schema + compatibility), lib/net (profile selection, per-profile cache,
   scan-for-known), lib/ha_client/app_logic (URL choice per mode/profile, per-SSID memory), lib/settings
   (two fields), settings_ui + render, tests.

## Acceptance criteria
- [ ] Spec approved; native tests for schema compatibility, profile selection, URL choice per mode and the
      "back to internal at home" path; builds pass.
- [ ] On device: home Wi-Fi → internal; hotspot → external; back home → internal again.
