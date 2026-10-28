#!/usr/bin/env python3
"""Host-side test of tools/provision.py against a fake device on a pty pair (task 020).

Run: python3 tools/tests/test_provision.py     (needs pyserial; takes about 15 s because of timeouts)
Only config.example.yaml and generated files are used. The stub speaks the device protocol of
src/provision.h: putfile line -> PUT-READY -> bytes with PUT-ACK every 1 KB -> PUT-OK / PUT-ERR.
"""
import os
import subprocess
import sys
import tempfile
import threading
import time
import tty
import unittest
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOL = os.path.join(ROOT, "tools", "provision.py")
EXAMPLE = os.path.join(ROOT, "config.example.yaml")


class FakeDevice(threading.Thread):
    """mode: ok | corrupt (flips a byte, so the CRC check fails) | silent (no PUT-ACK after READY)."""

    def __init__(self, mode="ok"):
        super().__init__(daemon=True)
        self.mode = mode
        self.master, self.slave = os.openpty()
        tty.setraw(self.slave)
        self.port = os.ttyname(self.slave)
        self.received = None
        self.rebooted = False
        self.stop = False

    def say(self, text):
        os.write(self.master, text.encode())

    def read_line(self):
        buf = b""
        while not self.stop:
            c = self._read(1)
            if c is None:
                continue
            if c == b"\n":
                return buf.decode()
            buf += c
        return None

    def _read(self, n):
        import select
        r, _, _ = select.select([self.master], [], [], 0.1)
        return os.read(self.master, n) if r else None

    def run(self):
        self.say("BOOT: start=noreset\nBOOT: settle_bst_end=1\n")
        while not self.stop:
            line = self.read_line()
            if line is None:
                return
            if line == "reboot":
                self.rebooted = True
                self.say("BOOT: start=noreset\nBOOT: config_source=sd\nBOOT: settle_bst_end=1\n")
            elif line.startswith("putfile "):
                self.putfile(line.split())

    def putfile(self, parts):
        _, name, size, crc = parts
        size = int(size)
        self.say("PUT-READY\n")
        data = b""
        while len(data) < size and not self.stop:
            chunk = self._read(min(256, size - len(data)))
            if chunk is None:
                continue
            data += chunk
            if len(data) % 1024 == 0 and len(data) < size and self.mode != "silent":
                self.say("PUT-ACK %d\n" % len(data))
        if len(data) < size:
            return
        if self.mode == "corrupt":
            data = bytes([data[0] ^ 1]) + data[1:]
        got = zlib.crc32(data) & 0xFFFFFFFF
        if "%08x" % got != crc:
            self.say("PUT-ERR crc-mismatch\n")
            return
        self.received = data
        summary = ("PUT-SECRETS ok wifi=yes ha=yes" if name == "secrets.yaml"
                   else "PUT-CONFIG ok tabs=1 widgets=1 connection=no")
        self.say("PUT-OK %d %08x\n%s\n" % (size, got, summary))


def run_tool(dev, args, timeout=40):
    env = dict(os.environ, XDG_RUNTIME_DIR=tempfile.mkdtemp())
    return subprocess.run([sys.executable, TOOL, "--port", dev.port, "--lock-wait", "2"] + args,
                          capture_output=True, text=True, timeout=timeout, env=env, input="")


class ProvisionTool(unittest.TestCase):
    def start(self, mode="ok"):
        dev = FakeDevice(mode)
        dev.start()
        self.addCleanup(lambda: setattr(dev, "stop", True))
        return dev

    def test_happy_path_test_name(self):
        dev = self.start()
        r = run_tool(dev, ["--name", "test-provision.yaml", EXAMPLE, "--yes"])
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        with open(EXAMPLE, "rb") as fh:
            want = fh.read()
        self.assertEqual(dev.received, want)
        self.assertIn("PUT-OK %d %08x" % (len(want), zlib.crc32(want) & 0xFFFFFFFF), r.stdout)
        self.assertIn("PUT-CONFIG ok", r.stdout)
        self.assertFalse(dev.rebooted)  # test-*.yaml never reboots
        self.assertNotIn(want.decode().splitlines()[0], r.stdout)  # content is never printed

    def test_happy_path_config_reboots(self):
        dev = self.start()
        r = run_tool(dev, ["--yes", EXAMPLE])
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        time.sleep(0.3)
        self.assertTrue(dev.rebooted)
        self.assertIn("config_source=sd", r.stdout)

    def test_secrets_name(self):
        dev = self.start()
        secrets = os.path.join(ROOT, "secrets.example.yaml")
        r = run_tool(dev, ["--name", "secrets.yaml", secrets, "--yes"])
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        with open(secrets, "rb") as fh:
            want = fh.read()
        self.assertEqual(dev.received, want)
        self.assertIn("PUT-SECRETS ok wifi=yes ha=yes", r.stdout)
        self.assertNotIn("PUT-CONFIG", r.stdout)
        self.assertNotIn("long-lived-token", r.stdout + r.stderr)  # content is never printed
        self.assertIn("rebooting", r.stdout)
        time.sleep(0.3)
        self.assertTrue(dev.rebooted)

    def test_crc_mismatch(self):
        dev = self.start("corrupt")
        r = run_tool(dev, ["--name", "test-provision.yaml", EXAMPLE, "--yes"])
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn("PUT-ERR crc-mismatch", r.stderr)
        self.assertIsNone(dev.received)

    def test_oversize_refused_without_device(self):
        dev = self.start()
        with tempfile.NamedTemporaryFile(suffix=".yaml") as f:
            f.write(b"# x\n" * (16 * 1024 + 1))  # 64 KB + 4 bytes
            f.flush()
            r = run_tool(dev, ["--name", "test-big.yaml", f.name, "--yes"])
        self.assertEqual(r.returncode, 2, r.stdout + r.stderr)
        self.assertIn("refused", r.stderr)
        self.assertIsNone(dev.received)

    def test_timeout_no_ack(self):
        dev = self.start("silent")
        with tempfile.NamedTemporaryFile(suffix=".yaml") as f:
            f.write(b"# generated\n" * 200)  # 2.4 KB: needs an ACK after 1 KB
            f.flush()
            r = run_tool(dev, ["--name", "test-slow.yaml", f.name, "--yes"])
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn("no PUT-ACK", r.stderr)

    def test_bad_name_refused(self):
        dev = self.start()
        r = run_tool(dev, ["--name", "../evil.yaml", EXAMPLE, "--yes"])
        self.assertNotEqual(r.returncode, 0)
        self.assertIsNone(dev.received)


if __name__ == "__main__":
    unittest.main(verbosity=2)
