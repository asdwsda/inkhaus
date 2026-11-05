// Provisioning protocol client for /install/ (task 119). Mirrors tools/provision.py and src/provision.h:
//   putfile <name> <size> <crc32 hex8> -> PUT-READY, raw bytes, PUT-ACK <offset> after every 1 KB,
//   PUT-OK <size> <crc>, PUT-CONFIG|PUT-SECRETS summary; reboot; getfile config.yaml -> GET-BEGIN <size> <crc>,
//   raw bytes, GET-END (config.yaml only; the device refuses every other name).
// No I/O of its own: it talks to a transport {write(Uint8Array): Promise, read(): Promise<Uint8Array|null>}.
// It never logs and never stores file content; secrets pass through putFile() and are not kept.
(function (root) {
  "use strict";
  var MAX = 64 * 1024, ACK_EVERY = 1024, PIECE = 256;
  var NAME_RE = /^(config\.yaml|secrets\.yaml)$/;

  var TABLE = null;
  function crc32(bytes) {
    if (!TABLE) {
      TABLE = new Uint32Array(256);
      for (var n = 0; n < 256; n++) {
        var c = n;
        for (var k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1;
        TABLE[n] = c >>> 0;
      }
    }
    var crc = 0xFFFFFFFF;
    for (var i = 0; i < bytes.length; i++) crc = TABLE[(crc ^ bytes[i]) & 0xFF] ^ (crc >>> 8);
    return (crc ^ 0xFFFFFFFF) >>> 0;
  }
  function hex8(v) { return ("00000000" + (v >>> 0).toString(16)).slice(-8); }

  function ProvisionError(code, message) {
    var e = new Error(message || code);
    e.name = "ProvisionError";
    e.code = code;  // port | no-answer | refused | timeout | unplugged | crc | protocol
    return e;
  }

  function Client(transport, opts) {
    opts = opts || {};
    this.t = transport;
    this.buf = new Uint8Array(0);
    this.pending = null;
    this.closed = false;
    this.idleMs = opts.idleMs || 3000;      // no byte for this long aborts a transfer
    this.answerMs = opts.answerMs || 4000;  // wait for PUT-READY / GET-BEGIN
    this.resultMs = opts.resultMs || 15000;
    this.attempts = opts.attempts || 4;
  }

  // Waits for more bytes (appended to this.buf as they arrive, even when nobody waits); returns true when some
  // arrived, null on timeout. A read that timed out stays pending, so no byte is lost.
  Client.prototype._fill = function (ms) {
    var self = this;
    if (this.closed) return Promise.reject(ProvisionError("unplugged", "connection closed"));
    if (!this.pending) {
      this.pending = Promise.resolve(this.t.read()).then(function (c) {
        self.pending = null;
        if (c === null || c === undefined) { self.closed = true; return "closed"; }
        var n = new Uint8Array(self.buf.length + c.length);
        n.set(self.buf); n.set(c, self.buf.length);
        self.buf = n;
        return true;
      }, function () { self.pending = null; self.closed = true; return "closed"; });
    }
    var timer;
    var timeout = new Promise(function (res) { timer = setTimeout(function () { res(null); }, ms); });
    return Promise.race([this.pending, timeout]).then(function (r) {
      clearTimeout(timer);
      if (r === "closed") throw ProvisionError("unplugged", "connection closed");
      return r;
    });
  };

  // One text line (without the line end) or null on timeout.
  Client.prototype.readLine = async function (ms) {
    var deadline = Date.now() + ms;
    for (;;) {
      var i = this.buf.indexOf(10);
      if (i >= 0) {
        var line = String.fromCharCode.apply(null, this.buf.subarray(0, i)).replace(/\r$/, "").trim();
        this.buf = this.buf.slice(i + 1);
        return line;
      }
      if (this.buf.length > 4096) this.buf = this.buf.slice(this.buf.length - 1024);  // log noise without newline
      var left = deadline - Date.now();
      if (left <= 0) return null;
      await this._fill(left);
    }
  };

  // Reads lines until one matches `re` (others are skipped: boot logs); null on timeout.
  Client.prototype.waitFor = async function (re, ms) {
    var deadline = Date.now() + ms;
    for (;;) {
      var left = deadline - Date.now();
      if (left <= 0) return null;
      var line = await this.readLine(left);
      if (line === null) return null;
      if (re.test(line)) return line;
    }
  };

  Client.prototype.discard = function () { this.buf = new Uint8Array(0); };
  Client.prototype._send = function (text) { return this.t.write(new TextEncoder().encode(text)); };

  // Exactly n raw bytes; every wait is bounded by the idle timeout.
  Client.prototype._readBytes = async function (n, onProgress) {
    var out = new Uint8Array(n), got = 0;
    for (;;) {
      var take = Math.min(this.buf.length, n - got);
      if (take) {
        out.set(this.buf.subarray(0, take), got);
        this.buf = this.buf.slice(take);
        got += take;
        if (onProgress) onProgress(got, n);
      }
      if (got === n) return out;
      if ((await this._fill(this.idleMs)) === null) throw ProvisionError("timeout", "no data for " + this.idleMs + " ms");
    }
  };

  // Resets the board (DTR+RTS together, like tools/provision.py) and waits for the end of a boot, if one runs.
  // `signals(dtr, rts)` is optional (Web Serial setSignals).
  Client.prototype.wake = async function (signals, opts) {
    opts = opts || {};
    if (signals) {
      try { await signals(true, true); await new Promise(function (r) { setTimeout(r, 200); }); await signals(false, false); } catch (e) { /* no control lines */ }
    }
    var seen = false, deadline = Date.now() + (opts.firstMs || 4000);
    for (;;) {
      var left = deadline - Date.now();
      if (left <= 0) break;
      var line = await this.readLine(left);
      if (line === null) break;
      if (line.indexOf("BOOT:") >= 0) { seen = true; deadline = Date.now() + (opts.bootMs || 25000); }
      if (seen && line.indexOf("settle_bst_end") >= 0) break;
    }
    if (seen) await new Promise(function (r) { setTimeout(r, opts.settleMs === undefined ? 300 : opts.settleMs); });
    this.discard();
    return seen;
  };

  // Reads /config.yaml from the device. Returns {bytes, crc}.
  Client.prototype.getFile = async function (name, onProgress) {
    if (name !== "config.yaml") throw ProvisionError("refused", "only config.yaml can be read");
    var resp = null;
    for (var a = 0; a < this.attempts; a++) {
      this.discard();
      await this._send("getfile config.yaml\n");
      resp = await this.waitFor(/^GET-(BEGIN|ERR)/, this.answerMs);
      if (resp !== null) break;
    }
    if (resp === null) throw ProvisionError("no-answer", "no answer from the device");
    var m = /^GET-BEGIN (\d+) ([0-9a-f]{8})$/.exec(resp);
    if (!m) throw ProvisionError(/^GET-ERR no-file/.test(resp) ? "no-file" : "refused", resp);
    var size = parseInt(m[1], 10), crc = parseInt(m[2], 16) >>> 0;
    if (size < 1 || size > MAX) throw ProvisionError("refused", "bad size");
    var bytes = await this._readBytes(size, onProgress);
    var end = await this.waitFor(/^GET-(END|ERR)/, this.idleMs);
    if (end !== "GET-END") throw ProvisionError("timeout", end || "no GET-END");
    if (crc32(bytes) !== crc) throw ProvisionError("crc", "checksum mismatch");
    return { bytes: bytes, crc: crc };
  };

  // Writes name (config.yaml | secrets.yaml). Returns {size, crc, summary}.
  Client.prototype.putFile = async function (name, bytes, onProgress) {
    if (!NAME_RE.test(name)) throw ProvisionError("refused", "name not allowed");
    if (bytes.length < 1 || bytes.length > MAX) throw ProvisionError("refused", "size must be 1 byte to 64 KB");
    var size = bytes.length, crc = crc32(bytes), resp = null;
    for (var a = 0; a < this.attempts; a++) {
      this.discard();
      await this._send("putfile " + name + " " + size + " " + hex8(crc) + "\n");
      resp = await this.waitFor(/^PUT-(READY|ERR)/, this.answerMs);
      if (resp !== null) break;
    }
    if (resp === null) throw ProvisionError("no-answer", "no answer from the device");
    if (resp.indexOf("PUT-ERR") === 0) throw ProvisionError("refused", resp);
    var sent = 0, nextAck = ACK_EVERY;
    while (sent < size) {
      var end = Math.min(sent + PIECE, size, nextAck);
      await this.t.write(bytes.subarray(sent, end));
      sent = end;
      if (sent === nextAck && sent < size) {
        var ack = await this.waitFor(/^PUT-(ACK|ERR)/, this.idleMs + 2000);
        if (ack === null) throw ProvisionError("timeout", "no PUT-ACK");
        if (ack.indexOf("PUT-ERR") === 0) throw ProvisionError("refused", ack);
        nextAck += ACK_EVERY;
      }
      if (onProgress) onProgress(sent, size);
    }
    var ok = await this.waitFor(/^PUT-(OK|ERR)/, this.resultMs);
    if (ok === null) throw ProvisionError("timeout", "no result from the device");
    if (ok.indexOf("PUT-ERR") === 0) throw ProvisionError("refused", ok);
    var m = /^PUT-OK (\d+) ([0-9a-f]{8})$/.exec(ok);
    if (!m || parseInt(m[1], 10) !== size || (parseInt(m[2], 16) >>> 0) !== crc) throw ProvisionError("crc", "the device reports a different size or checksum");
    var tag = name === "secrets.yaml" ? "PUT-SECRETS" : "PUT-CONFIG";
    var summary = await this.waitFor(new RegExp("^" + tag), 5000);
    return { size: size, crc: crc, summary: summary || "" };
  };

  Client.prototype.reboot = function () { return this._send("reboot\n"); };

  var api = { Client: Client, ProvisionError: ProvisionError, crc32: crc32, hex8: hex8, MAX_BYTES: MAX };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.ProvisionClient = api;
})(typeof self !== "undefined" ? self : this);
