#!/usr/bin/env python3
"""Local OTA test server (task 087). Serves manifest.json + firmware.bin over plain HTTP for a debug build
(-DDEBUG_SERIAL; set NVS ota/url or build with -DOTA_TEST_URL=\\"http://HOST:PORT/\\").

Usage: tools/ota_test_server.py --bin .pio/build/m5paper/firmware.bin --version 9.9.9 [--port 8070]
           [--corrupt]   # manifest keeps the real SHA-256, the served bytes are altered (device must reject)
"""
import argparse
import time
import hashlib
import http.server
import json


def make_manifest(data, version):
    return {"version": version, "chip": "esp32", "flash_size": "16MB",
            "ota": {"file": "firmware.bin", "size": len(data), "sha256": hashlib.sha256(data).hexdigest()}}


def make_handler(data, manifest, served, slow=0):
    class H(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path == "/manifest.json":
                body, ctype = json.dumps(manifest).encode(), "application/json"
            elif self.path == "/firmware.bin":
                body, ctype = served, "application/octet-stream"
            else:
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            if slow > 0 and self.path == "/firmware.bin":
                for i in range(0, len(body), slow):  # slow bytes per second
                    self.wfile.write(body[i:i + slow])
                    self.wfile.flush()
                    time.sleep(1)
            else:
                self.wfile.write(body)

    return H


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--version", required=True)
    ap.add_argument("--port", type=int, default=8070)
    ap.add_argument("--slow", type=int, default=0, metavar="BYTES_PER_S", help="throttle firmware.bin (task 090)")
    ap.add_argument("--corrupt", action="store_true")
    a = ap.parse_args()
    data = open(a.bin, "rb").read()
    served = data
    if a.corrupt:
        served = data[:1000] + bytes([data[1000] ^ 0xFF]) + data[1001:]
    m = make_manifest(data, a.version)
    print("serving %s v%s (%d bytes, corrupt=%s) on :%d" % (a.bin, a.version, len(data), a.corrupt, a.port))
    http.server.ThreadingHTTPServer(("", a.port), make_handler(data, m, served, a.slow)).serve_forever()


if __name__ == "__main__":
    main()
