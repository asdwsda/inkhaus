#!/usr/bin/env python3
"""Fetch the wake log (/wakelog.csv on the SD card) over serial (task 029).

Needs the debug_log setting on (or env m5paper-batterytest, which forces it). The device must be awake
(wake it, then run this within the idle time). Sends `wakelog`; the reply is
"WAKELOG-BEGIN <n>", n bytes, "WAKELOG-END" (or "WAKELOG-ERR <why>"). Attaches without resetting.

  tools/wakelog.py wakelog.csv
  tools/wakelog.py --file debug.log debug.log   # the key log lines (serial command `debuglog`)
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
    ap.add_argument("out", help="file to write")
    ap.add_argument("--file", choices=["wakelog.csv", "debug.log"], default="wakelog.csv", help="which SD file to fetch")
    ap.add_argument("--port", default="/dev/ttyUSB0")
    ap.add_argument("--timeout", type=float, default=15.0)
    a = ap.parse_args()

    with serial.Serial(a.port, 115200, timeout=0.2) as ser:
        ser.reset_input_buffer()
        ser.write(b"debuglog\n" if a.file == "debug.log" else b"wakelog\n")
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
                print(f"{a.out}: {n} bytes, {buf[start:start + n].count(b'\n')} lines")
                return
        sys.exit("no complete reply (is the debug log on, and is the device awake?)")


if __name__ == "__main__":
    main()
