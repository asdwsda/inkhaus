#!/usr/bin/env python3
"""Take a screenshot of the M5Paper UI over serial (task 017).

Sends `shotz` (run-length coded) or `shot` (raw) to the running app, which replies with
"SHOT-BEGIN <w> <h> [rle <n>]", the pixel data and "SHOT-END". The grey canvas is written as PNG
(Pillow) or PGM if Pillow is missing, or if the output name ends in .pgm.

Needs firmware flashed with `-e m5paper-debug` (the plain m5paper build has no shot/tap/key/pend).

It attaches without resetting the board. Test commands can be sent first, e.g. a tap and then the
screenshot while the fake action is still pending:

  tools/screenshot.py out.png
  tools/screenshot.py --send 'pend 60000' --send 'tap 300 330' --delay 1.5 out.png
  tools/screenshot.py --raw out.pgm

Commands the app understands: tap X Y, key up|down|push|long, pend MS, cleanup 0|1.

Debug views (task 017 fix round):
  --prev            fetch the presenter's previous frame (what we believe the panel shows)
  --mem X Y W H     fetch the IT8951 image memory of that screen rect (raw controller value in the
                    R channel; white = 0xF0 region, black = 0x00)
  --steps FILE      (with --mem) write several snapshots: for each --send step in turn, send the
                    command, wait --delay, then fetch memory; FILE_<n>.pgm, plus the canvas FILE_<n>_cur
                    and FILE_<n>_prev.
"""
import argparse
import re
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial is missing: python3 -m pip install --user pyserial")


def read_until_header(port, timeout):
    """Skips log lines; returns (w, h, rle_len or None) after the SHOT-BEGIN line."""
    deadline = time.monotonic() + timeout
    buf = b""
    while time.monotonic() < deadline:
        buf += port.read(256)
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            m = re.match(rb"SHOT-BEGIN (\d+) (\d+)(?: rle (\d+))?\s*$", line.strip())
            if m:
                return int(m[1]), int(m[2]), (int(m[3]) if m[3] else None), buf
            if line.strip():
                print("  dev:", line.decode("utf-8", "replace").rstrip(), file=sys.stderr)
    raise TimeoutError("no SHOT-BEGIN from the device")


def read_exact(port, n, have, timeout):
    data = bytearray(have)
    deadline = time.monotonic() + timeout
    while len(data) < n and time.monotonic() < deadline:
        data += port.read(min(8192, n - len(data)))
    if len(data) < n:
        raise TimeoutError(f"short read: {len(data)} of {n} bytes")
    return bytes(data[:n]), bytes(data[n:])


def fetch_mem(port, x, y, w, h):
    port.write(f"mem {x} {y} {w} {h}\n".encode())
    port.flush()
    deadline = time.monotonic() + 15
    buf = b""
    while time.monotonic() < deadline:
        buf += port.read(256)
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            m = re.match(rb"MEM-BEGIN (\d+) (\d+) (\d+) (\d+) rle (\d+)\s*$", line.strip())
            if m:
                n = int(m[5])
                comp, rest = read_exact(port, n, buf, 30)
                out = bytearray()
                for i in range(0, len(comp), 2):
                    out += bytes([comp[i + 1]]) * (comp[i] + 1)
                tail = rest + port.read(64)
                if b"bst_end=1" not in tail:
                    print("warning: MEM-END bst_end=1 not seen", file=sys.stderr)
                if len(out) != w * h:
                    sys.exit(f"mem decoded {len(out)} bytes, expected {w * h}")
                return bytes(out)
            if b"MEM-ERR" in line:
                sys.exit(line.decode())
    raise TimeoutError("no MEM-BEGIN from the device")


def fetch_shot(port, cmd):
    port.write((cmd + "\n").encode())
    port.flush()
    w, h, rle, rest = read_until_header(port, 10)
    comp, rest = read_exact(port, rle, rest, 30)
    out = bytearray()
    for i in range(0, len(comp), 2):
        out += bytes([comp[i + 1]]) * (comp[i] + 1)
    port.read(64)
    return w, h, bytes(out)


def save(path, w, h, pixels):
    try:
        from PIL import Image
        Image.frombytes("L", (w, h), pixels).save(path)
    except ImportError:
        with open(path.replace(".png", ".pgm"), "wb") as f:
            f.write(f"P5\n{w} {h}\n255\n".encode() + pixels)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out", help="output file (.png, or .pgm)")
    ap.add_argument("--port", default="/dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--raw", action="store_true", help="use the raw `shot` dump instead of `shotz`")
    ap.add_argument("--send", action="append", default=[], help="command to send first (repeatable)")
    ap.add_argument("--delay", type=float, default=1.0, help="seconds to wait after the commands")
    ap.add_argument("--prev", action="store_true", help="fetch the previous frame (shotp)")
    ap.add_argument("--mem", nargs=4, type=int, metavar=("X", "Y", "W", "H"),
                    help="fetch the IT8951 image memory of a rect")
    ap.add_argument("--steps", action="store_true",
                    help="with --mem: after each --send command, snapshot memory + cur + prev")
    args = ap.parse_args()

    port = serial.Serial(args.port, args.baud, timeout=0.3)
    with port:
        if args.mem:
            x, y, w, h = args.mem
            port.reset_input_buffer()
            base = re.sub(r"\.(png|pgm)$", "", args.out)
            cmds = args.send if args.steps else []
            if not args.steps:
                for cmd in args.send:
                    port.write((cmd + "\n").encode())
                    port.flush()
                    time.sleep(0.3)
                time.sleep(args.delay)
            for n in range(len(cmds) + 1):
                if n > 0:
                    port.write((cmds[n - 1] + "\n").encode())
                    port.flush()
                    time.sleep(args.delay)
                    port.reset_input_buffer()
                tag = f"{base}_{n}" if args.steps else base
                save(tag + "_mem.png", w, h, fetch_mem(port, x, y, w, h))
                if args.steps:
                    fw, fh, cur = fetch_shot(port, "shotz")
                    _, _, prev = fetch_shot(port, "shotp")
                    # crop the rect out of the full frames
                    for name, img in (("cur", cur), ("prev", prev)):
                        crop = b"".join(img[(y + r) * fw + x:(y + r) * fw + x + w] for r in range(h))
                        save(f"{tag}_{name}.png", w, h, crop)
                print(f"snapshot {n}: {tag}")
            return 0
        port.reset_input_buffer()
        for cmd in args.send:
            port.write((cmd + "\n").encode())
            port.flush()
            time.sleep(0.3)
        time.sleep(args.delay)
        # Device log lines that arrived meanwhile are shown, not discarded.
        for line in port.read(port.in_waiting).splitlines():
            if line.strip():
                print("  dev:", line.decode("utf-8", "replace").rstrip(), file=sys.stderr)
        port.write(b"shotp\n" if args.prev else (b"shot\n" if args.raw else b"shotz\n"))
        port.flush()
        w, h, rle, rest = read_until_header(port, 10)
        if rle is None:
            # P5 header: "P5\n<w> <h>\n255\n"
            hdr = f"P5\n{w} {h}\n255\n".encode()
            blob, rest = read_exact(port, len(hdr) + w * h, rest, 120)
            pixels = blob[len(hdr):]
        else:
            comp, rest = read_exact(port, rle, rest, 30)
            out = bytearray()
            for i in range(0, len(comp), 2):
                out += bytes([comp[i + 1]]) * (comp[i] + 1)
            pixels = bytes(out)
        if len(pixels) != w * h:
            sys.exit(f"decoded {len(pixels)} bytes, expected {w * h}")
        end = rest + port.read(64)
        if b"SHOT-END" not in end:
            print("warning: SHOT-END not seen", file=sys.stderr)

    if args.out.endswith(".pgm"):
        with open(args.out, "wb") as f:
            f.write(f"P5\n{w} {h}\n255\n".encode() + pixels)
    else:
        try:
            from PIL import Image
        except ImportError:
            args.out = re.sub(r"\.png$", "", args.out) + ".pgm"
            with open(args.out, "wb") as f:
                f.write(f"P5\n{w} {h}\n255\n".encode() + pixels)
            print("Pillow missing, wrote PGM", file=sys.stderr)
        else:
            Image.frombytes("L", (w, h), pixels).save(args.out)
    print(f"{args.out}: {w}x{h}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
