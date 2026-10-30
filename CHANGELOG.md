# Changelog

## 1.2.1 - 2026-10-30

### Installer
- Windows: the installer no longer opens an empty console window behind the app.

## 1.2.0 - 2026-10-30

### Dashboard
- **History graph widget**: `display: graph` shows a sensor's history right on the tab (line, step or bars, same options as the history popup). Default size 2x2. It updates on every wake and on the scheduled refresh for the visible tab, and the last graph stays on screen while the device sleeps.
- A changed graph is redrawn with a single screen refresh, so a wake takes less time.
- Demo mode is smaller (4 tabs) and still shows every widget type, including graph widgets.

### Fixes
- Fixed a rare hang at power-on (about 1 in 10 cold starts) that needed the 15 s watchdog to recover.

### Installer and docs
- Redesigned installer with a cleaner look. After installing (or after an error) "Back" returns to the version choice instead of reinstalling.
- The installer now has the same version number as the firmware.
- Simpler README with a 4-step start. All options are now in `docs/CONFIG.md`, and the build notes are in `docs/DEVELOPMENT.md`.
- A new animated tour of the screens on the README and the website.

## 1.1.0 - 2026-10-30

### Install and website
- **Inkhaus Installer**: a small desktop app for Windows, macOS and Linux that downloads a released firmware and installs it on the M5Paper over USB, with real progress. No PlatformIO or Python needed (see `docs/FLASHER.md`).
- The installer can also upload your `config.yaml` and `secrets.yaml` to the SD card over USB and restarts the M5Paper afterwards.
- Release builds of the firmware and the installer are produced automatically for every version tag.
- New one-page website for Inkhaus.
- The project is renamed from Inkling to Inkhaus (splash screen, README, logo).

### Dashboard
- History popup for sensors: long press or rocker push opens a 24 h line / step graph, or 7 day and daily bars from Home Assistant statistics.
- Brightness popup for dimmable lights: long press (or rocker long push), tap-to-set bar, -/+10 %, presets, Off.
- Sensor values show 1 decimal by default (`decimals` overrides).
- Demo mode now shows realistic sample charts.
- Landscape: a rule line between the tabs and the widgets.

### Network and settings
- Up to 4 Wi-Fi profiles in `secrets.yaml`, chosen automatically or forced in Settings; Home Assistant connection Auto / Internal / External, learned per network.
- Wi-Fi Auto reconnects to the last network without a scan.
- Settings reorganised into Display, Sleep & wake, Network and Tools pages.

### Power and display
- Faster wake: the screen reacts after about 1.6 s instead of 3.2 s, and Wi-Fi starts earlier.
- A bold "Zz" badge shows that the device is going to sleep, the same in every wake mode.
- The splash screen now shows on every cold boot (reset button, power loss, flashing), followed by a full redraw; a deep-sleep wake never shows it.
- Cleaner popups: they keep their grey tones, with a full refresh when they close.

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
- The splash shows on a cold boot only when the screen record is unknown (e.g. after flashing); a plain reset skips it (planned for 1.1.0).
- The first HTTPS request of a wake takes about 4 s via Cloudflare-hosted domains (TLS
  handshake); later requests of the same wake reuse the connection.
- The touch controller stays awake during sleep; the standby current is not yet measured.
- Covers without position show "-" instead of a percentage.
- Battery life has not been measured over weeks yet (target: about one month per charge).
- BLE instead of Wi-Fi is only a future idea (see docs/ARCHITECTURE.md).
