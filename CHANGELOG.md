# Changelog

## 1.0.0 - 2026-10-29

First release of Inkling, an e-ink Home Assistant remote for the M5Stack M5Paper.

### Dashboard and widgets
- Config-driven dashboard: tabs and widgets from `config.yaml` on the SD card (or sent over USB).
- Widgets for `cover`, `light`, `switch`, `automation`, `climate`, `sensor`, `binary_sensor`,
  `weather` and `vacuum`; entity groups (device-side lists, e.g. all shutters at once).
- Sensors can show an attribute plus up to two details, with unit override and rounding.
- Covers without position feedback (state `unknown`) work: optimistic Opening / Closing / Stopped.
- Portrait (2 columns) and landscape (4 or 5 columns per tab, `landscape_columns`); tabs with
  too many widgets split into sub-pages.
- Time zone as a POSIX TZ string (`timezone`).
- Config errors are reported with path and line number; the validated config is cached in flash.

### Home Assistant
- REST API with a long-lived token; one `POST /api/template` call fetches all states.
- Local-first URL with an optional external HTTPS URL as fallback (remembers which worked).
- HTTPS with the full Mozilla root bundle (works for Nabu Casa, Cloudflare, Let's Encrypt, ...).
- Fast taps: optimistic UI, explicit on/off actions, one keep-alive connection per wake,
  actions queued before Wi-Fi is up are sent first.
- Wi-Fi fast reconnect (cached BSSID and channel, optional static IP); NTP clock sync at most
  once per 24 h.

### Power and display
- Three wake modes: Button (default), Touch, Touch & Button. Deep sleep with the power hold.
- Scheduled refresh (off / 15 / 30 / 60 / 120 / 240 min), idle timeout 10 / 20 / 30 / 60 s.
- Last known states persist across sleep and power loss; the status bar shows the link phase
  and staleness; the sleep indicator shows how to wake the device.
- Clean e-ink handling: partial refreshes only where something changed, fast black/white
  feedback, a full refresh on page changes, cold-boot re-render without a flash.
- Boot without the IT8951 reset pulse (display start-up in about 70 ms on a restart).

### Settings and tools
- On-device Settings: orientation, wake mode, refresh interval, idle timeout, firmware and
  battery info, refresh now.
- Tools page: Demo mode (built-in showcase, no Home Assistant needed) and Debug log
  (`/wakelog.csv` and `/debug.log` on the SD card).
- Splash screen on a cold boot.
- `secrets.yaml` (Wi-Fi, HA URLs, token) separate from `config.yaml` (dashboard).
- Host tools: `tools/provision.py` (send config/secrets over USB), `tools/wakelog.py` (read logs
  from the SD card), `tools/screenshot.py`, `tools/serial_capture.py`.

### Known limitations
- The first HTTPS request of a wake takes about 4 s via Cloudflare-hosted domains (TLS
  handshake); later requests of the same wake reuse the connection.
- The touch controller stays awake during sleep; the standby current is not yet measured.
- Covers without position show "-" instead of a percentage.
- Battery life has not been measured over weeks yet (target: about one month per charge).
- BLE instead of Wi-Fi is only a future idea (see docs/ARCHITECTURE.md).
