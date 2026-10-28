#!/usr/bin/env python3
"""Fetch the wake log (/wakelog.csv on the SD card) over serial (task 029).

Needs a firmware built with -DWAKE_LOG_SD (env m5paper-batterytest). The device must be awake
(wake it, then run this within the idle time). Sends `wakelog`; the reply is
"WAKELOG-BEGIN <n>", n bytes, "WAKELOG-END" (or "WAKELOG-ERR <why>"). Attaches without resetting.

  tools/wakelog.py wakelog.csv
"""
import argparse
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial is missing: python3 -m pip install --user pyserial")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out", help="CSV file to write")
    ap.add_argument("--port", default="/dev/ttyUSB0")
    ap.add_argument("--timeout", type=float, default=15.0)
    a = ap.parse_args()

    with serial.Serial(a.port, 115200, timeout=0.2) as ser:
        ser.reset_input_buffer()
        ser.write(b"wakelog\n")
        buf = b""
        deadline = time.monotonic() + a.timeout
        while time.monotonic() < deadline:
            buf += ser.read(4096)
            i = buf.find(b"WAKELOG-ERR")
            if i >= 0 and b"\n" in buf[i:]:
                sys.exit(buf[i:].split(b"\n")[0].decode(errors="replace"))
            b = buf.find(b"WAKELOG-BEGIN ")
            if b < 0:
                continue
            eol = buf.find(b"\n", b)
            if eol < 0:
                continue
            n = int(buf[b + 14:eol])
            start = eol + 1
            if len(buf) - start >= n:
                with open(a.out, "wb") as f:
                    f.write(buf[start:start + n])
                print(f"{a.out}: {n} bytes, {buf[start:start + n].count(b'\n') - 1} wakes")
                return
        sys.exit("no complete reply (is this a -DWAKE_LOG_SD build, and is the device awake?)")


if __name__ == "__main__":
    main()
