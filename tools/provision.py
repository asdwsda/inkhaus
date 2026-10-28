#!/usr/bin/env python3
"""Send a YAML file to the M5Paper's SD card over USB serial (task 020).

  tools/provision.py FILE                          # as /config.yaml, asks before rebooting
  tools/provision.py --name secrets.yaml FILE      # Wi-Fi + HA token; only "PUT-SECRETS ok wifi=yes ha=yes" is shown
  tools/provision.py --name test-provision.yaml config.example.yaml
  tools/provision.py --dry-run FILE                # size + CRC32 + limit check, no device access

The file content is never printed or echoed; only name, size, CRC and the device's replies.
The tool takes the device lock itself (flock on ${XDG_RUNTIME_DIR:-/tmp}/m5paper-device.lock,
the same file tools/device_lock.sh uses), so don't wrap it in device_lock.sh.

Protocol (firmware: src/provision.h): putfile <name> <size> <crc32>, PUT-READY, raw bytes with a
PUT-ACK <offset> after every 1 KB, then PUT-OK / PUT-ERR and PUT-CONFIG; `reboot` restarts.
Exit codes: 0 ok, 1 device/transfer error, 2 usage or refused file, 75 lock busy.
"""
import argparse
import fcntl
import os
import re
import sys
import time
import zlib

MAX_BYTES = 64 * 1024
ACK_EVERY = 1024
PIECE = 256  # bytes per write; the firmware reads in 256-byte pieces
NAME_RE = re.compile(r"^(config\.yaml|secrets\.yaml|test-[A-Za-z0-9._-]+\.yaml)$")


def lock_path():
    return os.path.join(os.environ.get("XDG_RUNTIME_DIR") or "/tmp", "m5paper-device.lock")


def take_lock(wait_s):
    f = open(lock_path(), "a+")
    deadline = time.time() + wait_s
    while True:
        try:
            fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
            break
        except OSError:
            if time.time() > deadline:
                f.seek(0)
                sys.exit("device lock busy for %ds; held by: %s" % (wait_s, f.read().strip()))
            time.sleep(0.5)
    f.seek(0)
    f.truncate()
    f.write("%s pid=%d cmd=%s\n" % (time.strftime("%Y-%m-%dT%H:%M:%S"), os.getpid(), " ".join(
        ["provision.py"] + [a for a in sys.argv[1:] if a.startswith("--")])))
    f.flush()
    return f  # keep open: the lock lives as long as this object


def read_line(ser, deadline):
    buf = b""
    while time.time() < deadline:
        c = ser.read(1)
        if not c:
            continue
        if c == b"\n":
            return buf.decode("ascii", "replace").strip()
        buf += c
    return None


def wait_for(ser, pattern, timeout, show=None):
    """Read lines until one matches; returns the matching line or None on timeout."""
    deadline = time.time() + timeout
    while True:
        line = read_line(ser, deadline)
        if line is None:
            return None
        if show and show(line):
            print("  " + line)
        if re.search(pattern, line):
            return line


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("file")
    ap.add_argument("--port", default="/dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--name", default="config.yaml", help="config.yaml, secrets.yaml or test-*.yaml")
    ap.add_argument("--dry-run", action="store_true", help="compute size and CRC32 only, no device access")
    ap.add_argument("--yes", action="store_true", help="reboot without asking")
    ap.add_argument("--no-reboot", action="store_true", help="never reboot")
    ap.add_argument("--lock-wait", type=int, default=300)
    args = ap.parse_args()

    if not NAME_RE.match(args.name) or len(args.name) > 40:
        sys.exit("refused: --name must be config.yaml, secrets.yaml or test-<name>.yaml")
    try:
        size = os.path.getsize(args.file)
    except OSError as e:
        sys.exit("cannot read %s: %s" % (args.file, e.strerror))
    if size > MAX_BYTES:
        print("refused: %s is %d bytes, the limit is %d (64 KB)" % (args.file, size, MAX_BYTES), file=sys.stderr)
        return 2
    if size == 0:
        print("refused: file is empty", file=sys.stderr)
        return 2
    with open(args.file, "rb") as f:
        data = f.read()
    crc = zlib.crc32(data) & 0xFFFFFFFF
    print("file: %s as /%s, size=%d crc32=%08x" % (os.path.basename(args.file), args.name, size, crc))
    if args.dry_run:
        print("dry run: ok, nothing sent")
        return 0

    try:
        import serial
    except ImportError:
        sys.exit("pyserial is missing: python3 -m pip install --user pyserial")

    lock = take_lock(args.lock_wait)  # noqa: F841 (held until exit)
    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.1)
    except serial.SerialException as e:
        sys.exit("cannot open %s: %s" % (args.port, e))
    with ser:
        # Opening the port may reset the board. Wait for the end of a boot if one is running; if no
        # boot output shows up, the board was already running.
        print("waiting for the device (a boot may be in progress) ...")
        ser.reset_input_buffer()
        seen_boot = False
        deadline = time.time() + 4
        while time.time() < deadline:
            line = read_line(ser, deadline)
            if line is None:
                break
            if "BOOT:" in line:
                seen_boot = True
                deadline = time.time() + 25
                if "settle_bst_end" in line:
                    break
        if seen_boot:
            print("boot finished")
            time.sleep(0.3)
        ser.reset_input_buffer()

        ser.write(("putfile %s %d %08x\n" % (args.name, size, crc)).encode())
        ser.flush()
        resp = wait_for(ser, r"^PUT-(READY|ERR)", 5)
        if resp is None:
            print("no answer from the device (is the firmware new enough?)", file=sys.stderr)
            return 1
        if resp.startswith("PUT-ERR"):
            print(resp, file=sys.stderr)
            return 1

        sent = 0
        next_ack = ACK_EVERY
        while sent < size:
            end = min(sent + PIECE, size, next_ack)
            ser.write(data[sent:end])
            sent = end
            if sent == next_ack and sent < size:
                ack = wait_for(ser, r"^PUT-(ACK|ERR)", 5)
                if ack is None or ack.startswith("PUT-ERR"):
                    print("\n" + (ack or "no PUT-ACK from the device"), file=sys.stderr)
                    return 1
                next_ack += ACK_EVERY
            sys.stdout.write("\rsent %d / %d bytes" % (sent, size))
            sys.stdout.flush()
        ser.flush()
        print()

        ok = wait_for(ser, r"^PUT-(OK|ERR)", 15)
        if ok is None or ok.startswith("PUT-ERR"):
            print(ok or "no result from the device", file=sys.stderr)
            return 1
        print(ok)
        m = re.match(r"PUT-OK (\d+) ([0-9a-f]{8})", ok)
        if not m or int(m.group(1)) != size or int(m.group(2), 16) != crc:
            print("device reports a different size/CRC than the local file", file=sys.stderr)
            return 1
        secrets = args.name == "secrets.yaml"
        tag = "PUT-SECRETS" if secrets else "PUT-CONFIG"
        summary = wait_for(ser, r"^" + tag, 5)
        print(summary or "no %s summary" % tag)
        config_bad = bool(summary and (tag + " error") in summary)
        if config_bad:
            print("warning: the device could not use this config as is")

        if args.no_reboot or args.name not in ("config.yaml", "secrets.yaml"):
            return 0
        if not args.yes:
            try:
                answer = input("Reboot the device now? [Y/n] ").strip().lower()
            except EOFError:
                answer = "n"
            if answer not in ("", "y", "yes"):
                print("not rebooting; run again with --yes or press reset to use the new config")
                return 0
        ser.reset_input_buffer()
        ser.write(b"reboot\n")
        ser.flush()
        print("rebooting; boot result:")
        shown = lambda l: bool(re.search(r"config_source|config_error|config_|settle_bst_end|setup_done", l))
        line = wait_for(ser, r"settle_bst_end", 30, shown)
        if line is None:
            print("no end of boot seen within 30 s", file=sys.stderr)
            return 1
        return 0


if __name__ == "__main__":
    sys.exit(main())
