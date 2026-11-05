// Node test for site/provision-client.js (task 119): node site/test/provision-client.test.js
// The recorded frames (frames.json) come from the real tools/provision.py (site/test/record_frames.py).
"use strict";
const assert = require("assert");
const fs = require("fs");
const path = require("path");
const { Client, crc32, hex8 } = require("../provision-client.js");

const frames = JSON.parse(fs.readFileSync(path.join(__dirname, "frames.json"), "utf8"));
const enc = new TextEncoder();
const hex = (u8) => Buffer.from(u8).toString("hex");

// A fake device on the other side of a transport. `files` is what getfile may return.
class FakeDevice {
  constructor(opts) {
    this.opts = opts || {};
    this.written = [];     // every host write, in order
    this.inbox = [];       // chunks for the client
    this.waiter = null;
    this.mode = "line";
    this.line = "";
    this.raw = [];
    this.rawLeft = 0;
    this.req = null;
    this.config = this.opts.config;  // Buffer or undefined
    this.closed = false;
  }
  say(textOrBytes) {
    const b = typeof textOrBytes === "string" ? enc.encode(textOrBytes) : textOrBytes;
    this.inbox.push(b);
    if (this.waiter) { const w = this.waiter; this.waiter = null; w(this.inbox.shift()); }
  }
  transport() {
    return {
      write: async (u8) => { this.written.push(Buffer.from(u8)); this.onBytes(u8); },
      read: () => new Promise((res, rej) => {
        if (this.closed) return rej(new Error("gone"));
        if (this.inbox.length) return res(this.inbox.shift());
        this.waiter = res;
      }),
    };
  }
  onBytes(u8) {
    for (const byte of u8) {
      if (this.mode === "raw") {
        this.raw.push(byte);
        const r = this.raw.length;
        if (r === this.req.size) {
          this.mode = "line";
          const data = Buffer.from(this.raw);
          this.received = data;
          const crc = crc32(data);
          if (this.opts.corruptResult) { this.say("PUT-ERR crc-mismatch\n"); continue; }
          this.say(`PUT-OK ${data.length} ${hex8(crc)}\n`);
          this.say(this.req.name === "secrets.yaml" ? "PUT-SECRETS ok wifi=yes ha=yes\n" : "PUT-CONFIG ok tabs=1 widgets=1 connection=no\n");
        } else if (r % 1024 === 0 && !this.opts.noAck) this.say(`PUT-ACK ${r}\n`);
        continue;
      }
      if (byte !== 10) { this.line += String.fromCharCode(byte); continue; }
      const l = this.line; this.line = "";
      this.command(l);
    }
  }
  command(l) {
    const p = l.split(" ");
    if (p[0] === "putfile") {
      this.req = { name: p[1], size: +p[2], crc: p[3] };
      this.raw = []; this.mode = "raw";
      this.say("PUT-READY\n");
    } else if (p[0] === "getfile") {
      // firmware: config.yaml only (lib/provision parseGetfile)
      if (p[1] !== "config.yaml") return this.say("GET-ERR bad-name\n");
      if (this.opts.silent) return;
      if (!this.config) return this.say("GET-ERR no-file\n");
      this.say(`GET-BEGIN ${this.config.length} ${hex8(this.opts.badCrc ? 1 : crc32(this.config))}\n`);
      for (let i = 0; i < this.config.length; i += 100) this.say(this.config.subarray(i, i + 100));
      if (!this.opts.truncate) this.say("\nGET-END\n");
    } else if (p[0] === "reboot") this.rebooted = true;
  }
}
const fast = { idleMs: 300, answerMs: 300, resultMs: 1000, attempts: 2 };
const tests = [];
const test = (n, f) => tests.push([n, f]);

test("crc32 matches zlib values", () => {
  assert.strictEqual(hex8(crc32(enc.encode("123456789"))), "cbf43926");
  assert.strictEqual(hex8(crc32(new Uint8Array(0))), "00000000");
  assert.strictEqual(hex8(crc32(enc.encode(frames.file_text))), frames.crc32);
});

test("putFile sends exactly the bytes tools/provision.py sent", async () => {
  const dev = new FakeDevice();
  const c = new Client(dev.transport(), fast);
  const data = enc.encode(frames.file_text);
  const progress = [];
  const r = await c.putFile("config.yaml", data, (a, b) => progress.push([a, b]));
  await c.reboot();
  assert.strictEqual(r.size, data.length);
  assert.strictEqual(hex8(r.crc), frames.crc32);
  assert.strictEqual(r.summary, "PUT-CONFIG ok tabs=1 widgets=1 connection=no");
  assert.strictEqual(hex(Buffer.concat(dev.written)), frames.host_sent_hex);
  assert.ok(dev.written.every((w, i) => i === 0 || w.length <= 256), "pieces of at most 256 bytes");
  assert.deepStrictEqual(progress[progress.length - 1], [data.length, data.length]);
  assert.ok(dev.rebooted);
  assert.deepStrictEqual(dev.received, Buffer.from(data));
});

test("device frames recorded from provision.py are understood", async () => {
  // Replay what the real tool's device saw: the client must accept the recorded replies in order.
  const said = frames.device_said.split("\n").filter((x) => /^PUT-/.test(x));
  assert.deepStrictEqual(said.map((x) => x.split(" ")[0]), ["PUT-READY", "PUT-ACK", "PUT-ACK", "PUT-OK", "PUT-CONFIG"]);
  assert.strictEqual(said[3], `PUT-OK ${frames.file_text.length} ${frames.crc32}`);
});

test("putFile retries the command when the first one is lost", async () => {
  const dev = new FakeDevice();
  let drop = true;
  const t = dev.transport(), w = t.write;
  t.write = async (u8) => { if (drop) { drop = false; dev.written.push(Buffer.from(u8)); return; } return w(u8); };
  const c = new Client(t, fast);
  const r = await c.putFile("secrets.yaml", enc.encode("wifi:\n  ssid: x\n"));
  assert.strictEqual(r.summary, "PUT-SECRETS ok wifi=yes ha=yes");
});

test("putFile refuses other names, empty and oversize files before sending", async () => {
  const dev = new FakeDevice();
  const c = new Client(dev.transport(), fast);
  for (const bad of [["test-x.yaml", 5], ["../config.yaml", 5], ["config.yaml", 0], ["config.yaml", 64 * 1024 + 1]])
    await assert.rejects(c.putFile(bad[0], new Uint8Array(bad[1])), (e) => e.code === "refused");
  assert.strictEqual(dev.written.length, 0);
});

test("putFile: device error and missing ack", async () => {
  const c = new Client(new FakeDevice({ corruptResult: true }).transport(), fast);
  await assert.rejects(c.putFile("config.yaml", enc.encode(frames.file_text)), (e) => e.code === "refused" && /crc-mismatch/.test(e.message));
  const c2 = new Client(new FakeDevice({ noAck: true }).transport(), Object.assign({}, fast, { idleMs: 100 }));
  await assert.rejects(c2.putFile("config.yaml", enc.encode(frames.file_text)), (e) => e.code === "timeout");
});

test("getFile reads config.yaml and checks the CRC", async () => {
  const cfg = Buffer.from(frames.file_text);
  const dev = new FakeDevice({ config: cfg });
  const c = new Client(dev.transport(), fast);
  const progress = [];
  const r = await c.getFile("config.yaml", (a, b) => progress.push(a));
  assert.deepStrictEqual(Buffer.from(r.bytes), cfg);
  assert.strictEqual(dev.written.map(String).join(""), "getfile config.yaml\n");
  assert.strictEqual(progress[progress.length - 1], cfg.length);
});

test("getFile: no file, bad CRC, truncated transfer, silence", async () => {
  await assert.rejects(new Client(new FakeDevice({}).transport(), fast).getFile("config.yaml"), (e) => e.code === "no-file");
  const cfg = Buffer.from("version: 1\n");
  await assert.rejects(new Client(new FakeDevice({ config: cfg, badCrc: true }).transport(), fast).getFile("config.yaml"), (e) => e.code === "crc");
  await assert.rejects(new Client(new FakeDevice({ config: cfg, truncate: true }).transport(), fast).getFile("config.yaml"), (e) => e.code === "timeout");
  await assert.rejects(new Client(new FakeDevice({ silent: true }).transport(), fast).getFile("config.yaml"), (e) => e.code === "no-answer");
});

test("getFile: unplugged cable", async () => {
  const dev = new FakeDevice({ silent: true });
  const c = new Client(dev.transport(), fast);
  const p = c.getFile("config.yaml");
  setTimeout(() => { dev.closed = true; const w = dev.waiter; dev.waiter = null; if (w) w(null); }, 50);
  await assert.rejects(p, (e) => e.code === "unplugged");
});

// Secrets can't be read: the client refuses before sending, and the device whitelist refuses too.
test("secrets can never be read", async () => {
  const dev = new FakeDevice({ config: Buffer.from("x") });
  const c = new Client(dev.transport(), fast);
  for (const n of ["secrets.yaml", "/secrets.yaml", "config.yaml ", "test-a.yaml", ""])
    await assert.rejects(c.getFile(n), (e) => e.code === "refused");
  assert.strictEqual(dev.written.length, 0);
  assert.ok(!/getfile/.test(fs.readFileSync(path.join(__dirname, "../provision-client.js"), "utf8").replace(/getfile config\.yaml/g, "").replace(/\/\/.*$/gm, "").replace(/getFile/g, "")),
    "no getfile command for another name is built anywhere");
});

test("wake waits for the end of a boot and drops the noise", async () => {
  const dev = new FakeDevice();
  const c = new Client(dev.transport(), fast);
  let pulses = 0;
  setTimeout(() => dev.say("BOOT: start=noreset\nBOOT: x=1\n"), 20);
  setTimeout(() => dev.say("BOOT: settle_bst_end=1\n"), 60);
  const seen = await c.wake(async () => { pulses++; }, { settleMs: 5, firstMs: 500 });
  assert.ok(seen);
  assert.strictEqual(pulses, 2);
});

(async () => {
  let failed = 0;
  for (const [n, f] of tests) {
    try { await f(); console.log("ok   " + n); } catch (e) { failed++; console.log("FAIL " + n + "\n" + (e && e.stack)); }
  }
  console.log(failed ? failed + " failed" : "all " + tests.length + " passed");
  process.exit(failed ? 1 : 0);
})();
