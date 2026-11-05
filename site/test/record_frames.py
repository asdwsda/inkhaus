#!/usr/bin/env python3
"""Records the byte stream tools/provision.py sends and receives (task 119), for site/test/provision-client.test.js.

Runs the real tools/provision.py against the fake device of tools/tests/test_provision.py on a pty and writes
site/test/frames.json: the file, what the host sent (hex), what the device said (text). Needs pyserial.
  python3 site/test/record_frames.py        # rewrite frames.json (committed; rerun when the protocol changes)
Uses a generated, non-secret text file of about 2.5 KB.
"""
import json, os, subprocess, sys, tempfile, zlib
from pathlib import Path

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / "tools" / "tests"))
import test_provision as tp  # noqa: E402


class Recorder(tp.FakeDevice):
    def __init__(self):
        super().__init__("ok")
        self.host = bytearray()
        self.dev = []

    def say(self, text):
        self.dev.append(text)
        super().say(text)

    def _read(self, n):
        c = super()._read(n)
        if c:
            self.host += c
        return c


def main():
    lines = ["version: 1", "tabs:", "  - title: Test", "    widgets:"]
    for i in range(80):
        lines.append("      - entity: light.fake_%02d" % i)
    data = ("\n".join(lines) + "\n").encode()
    assert 2048 < len(data) < 4096, len(data)
    f = tempfile.NamedTemporaryFile(suffix=".yaml", delete=False)
    f.write(data)
    f.close()
    dev = Recorder()
    dev.start()
    env = dict(os.environ, XDG_RUNTIME_DIR=tempfile.mkdtemp())
    r = subprocess.run([sys.executable, str(root / "tools/provision.py"), "--port", dev.port, "--lock-wait", "2",
                        "--yes", f.name], capture_output=True, text=True, timeout=60, env=env, input="")
    dev.stop = True
    os.unlink(f.name)
    if r.returncode != 0:
        sys.exit(r.stdout + r.stderr)
    out = {
        "name": "config.yaml",
        "file_text": data.decode(),
        "crc32": "%08x" % (zlib.crc32(data) & 0xFFFFFFFF),
        "host_sent_hex": bytes(dev.host).hex(),
        "device_said": "".join(dev.dev),
    }
    (Path(__file__).parent / "frames.json").write_text(json.dumps(out, indent=1) + "\n")
    print("wrote frames.json: host %d bytes, device %d chars" % (len(dev.host), len(out["device_said"])))


if __name__ == "__main__":
    main()
