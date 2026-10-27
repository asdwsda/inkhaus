#!/usr/bin/env python3
"""Capture the M5Paper's serial output without an interactive terminal.

`pio device monitor` needs a TTY on stdin, so agents can't use it. This reads the
port directly with pyserial, optionally resets the board first, and prints each line
with a timestamp (seconds since the capture started).

Reset: on this CP2104 board only "DTR and RTS asserted together, then both released"
resets the ESP32 into the app (the esptool-style pulse doesn't). Every reset re-runs
setup(), which on our firmware means a full e-ink refresh (a visible flash).

Examples:
  tools/serial_capture.py --reset --seconds 20
  tools/serial_capture.py --reset --seconds 300 --until 'PROBE: done'
  tools/serial_capture.py --seconds 60            # attach without resetting

Exit code: 0 normally, 2 if --until was given and never matched, 1 on port errors.
"""
import argparse
import re
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial is missing: python3 -m pip install --user pyserial")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--seconds", type=float, default=20, help="maximum capture time")
    ap.add_argument("--reset", action="store_true", help="reset the board before capturing")
    ap.add_argument("--until", help="stop early once a line matches this regex")
    args = ap.parse_args()

    until = re.compile(args.until) if args.until else None
    try:
        port = serial.Serial(args.port, args.baud, timeout=0.2)
    except serial.SerialException as exc:
        print(f"cannot open {args.port}: {exc}", file=sys.stderr)
        return 1

    with port:
        if args.reset:
            port.dtr = True
            port.rts = True
            time.sleep(0.2)
            port.dtr = False
            port.rts = False
        start = time.monotonic()
        pending = b""
        matched = False
        while time.monotonic() - start < args.seconds:
            pending += port.read(4096)
            while b"\n" in pending:
                raw, pending = pending.split(b"\n", 1)
                line = raw.rstrip(b"\r").decode("utf-8", "replace")
                print(f"[{time.monotonic() - start:7.2f}] {line}", flush=True)
                if until and until.search(line):
                    matched = True
                    break
            if matched:
                break
        if pending and not matched:
            print(f"[{time.monotonic() - start:7.2f}] {pending.decode('utf-8', 'replace')}", flush=True)

    if until and not matched:
        print(f"--until pattern never matched: {args.until}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
