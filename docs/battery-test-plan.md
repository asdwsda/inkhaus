# Battery test plan

Goal: real numbers for the 1-month target (ARCHITECTURE.md: ~1.3 mA average budget, ~900 mAh
usable of the 1150 mAh cell). Owned by the architect; run when the device is back.

## What we need to know
| Quantity | Why | Target / estimate |
|---|---|---|
| Off current (Button mode, powered off) | floor of the whole budget | tens of µA |
| Deep-sleep current (Touch mode) | decides whether Touch mode can reach a month | est. 1–2 mA |
| Charge per interactive wake | dominates with daily use | est. ~0.5 mAh |
| Charge per scheduled refresh | × 24/day at the 60 min default | est. ~0.1 mAh |
| Awake time per wake, split by phase | where to optimise | from the logs |

## Method A: from the firmware logs (no extra hardware)
Every sleep logs `SLEEP: … battery_mv=… awake_ms=…`, and every boot logs timings
(`display_start_ms`, `NET: connect_ms`, `fetch_done`, TLS time).
1. Fully charge, unplug USB, and use the device normally with the default settings (Button mode,
   60 min refresh) for 3–7 days.
2. Plug in briefly once a day only to read the serial log (`tools/serial_capture.py`, no reset needed
   right after a wake), or better: a test build (`-DWAKE_LOG_SD`) appends one CSV line per wake to
   `/wakelog.csv` on the SD card (epoch, wake reason, battery mV, awake ms per phase, refresh
   count). The card has plenty of space and its own wear levelling; read it out at the end with
   `tools/provision.py`-style serial fetch or by taking the card out. Only in the test build: an SD
   mount costs ~20–100 ms and card power on every wake (user suggestion to use the SD card).
3. Battery voltage over time → % via `power::battery_percent` → mAh/day. Combine with the wake
   counts and `awake_ms` per phase.
Limit: the voltage curve is coarse; good for days, not for single wakes.

## Method B: with a USB power meter (recommended, ~10 € inline USB-C meter)
The meter must show mA and accumulate mAh. With USB connected the board doesn't power off, so:
1. **Awake phases:** measure on USB with the battery present. Note mA during boot (display start),
   Wi-Fi connect, TLS handshake, fetch, e-ink refresh, idle with the screen static. Read the mAh
   counter before and after 10 interactive wakes and 10 scheduled refreshes → mAh per wake.
2. **Sleep floors:** USB input can't show the battery's sleep current. Use a multimeter in series with
   the battery (JST connector) in µA/mA range for Button mode off and Touch mode deep sleep.
   (Only if the user is comfortable opening the case; otherwise rely on Method A for floors.)

## Log lines to check before the test (all exist)
- `SLEEP: mode=… alarm_at=… battery_mv=… awake_ms=…`
- `BOOT: display_start_ms=…`, `NET: connect_ms=… cache=hit|miss`, `APP: fetch_done … link=…`
- TLS time per request (add `ms=` to the transport log if missing — check before the session)

## Decisions this test feeds
- Default refresh interval (60 min) and idle timeout (20 s).
- Whether Touch mode gets a stronger warning, or keeps the EPD controller in standby during sleep.
- Whether TLS session resumption (task 027) and BLE (ARCHITECTURE.md future ideas) are worth it.
