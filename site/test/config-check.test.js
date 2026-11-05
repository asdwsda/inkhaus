// Node test for site/config-check.js (task 119): node site/test/config-check.test.js
"use strict";
const assert = require("assert");
const fs = require("fs");
const path = require("path");
const C = require("../config-check.js");
const root = path.join(__dirname, "..", "..");
const msgs = (t, sev) => C.checkConfig(t).filter((m) => !sev || m.severity === sev).map((m) => m.line + ":" + m.text);
const GOOD = "version: 1\ntabs:\n  - title: A\n    widgets:\n      - entity: light.kitchen\n";
const tests = [];
const test = (n, f) => tests.push([n, f]);

test("the shipped examples have no errors", () => {
  for (const f of ["config.example.yaml", "demo.yaml"]) {
    const e = C.checkConfig(fs.readFileSync(path.join(root, f), "utf8")).filter((m) => m.severity === "error");
    assert.deepStrictEqual(e, [], f);
  }
});
test("a minimal file is clean", () => assert.deepStrictEqual(C.checkConfig(GOOD), []));
test("version, tabs", () => {
  assert.ok(msgs("tabs:\n  - title: A\n    widgets:\n      - entity: light.a\n", "error")[0].includes("'version' is missing"));
  assert.ok(msgs("version: 1\n", "error")[0].includes("No tabs"));
  assert.ok(msgs("version: 1\ntabs: []\n", "error")[0].includes("No tabs"));
  let many = "version: 1\ntabs:\n";
  for (let i = 0; i < 17; i++) many += "  - title: T" + i + "\n    widgets:\n      - entity: light.a" + i + "\n";
  assert.ok(msgs(many, "error").some((m) => m.includes("Too many tabs (max 16)")));
  let w = "version: 1\ntabs:\n  - title: T\n    widgets:\n";
  for (let i = 0; i < 25; i++) w += "      - entity: light.a" + i + "\n";
  assert.ok(msgs(w, "error").some((m) => m.includes("Too many widgets in this tab (max 24)")));
});
test("syntax errors", () => {
  assert.ok(msgs("version: 1\ntabs:\n\t- title: A\n", "error")[0].includes("YAML syntax error: tab character"));
  assert.ok(msgs('version: 1\nname: "open\n', "error")[0].includes("unclosed quote"));
  assert.ok(msgs("version: 1\nversion: 2\n", "error")[0].includes("duplicate key"));
  const m = msgs("version: 1\ntabs\n", "error")[0];
  assert.ok(m.startsWith("2:Missing ':' after 'tabs'"), m);
  assert.ok(m.includes("Write it as 'tabs: value'."));
  assert.ok(msgs("version: 1\ntabs:\n  - title: A\n      widgets: []\n", "error")[0].startsWith("4:"));
});
test("entity ids and domains", () => {
  const f = (e) => msgs(GOOD.replace("light.kitchen", e), "error");
  assert.ok(f("kitchen")[0].includes("Not an entity id: 'kitchen'. Use domain.name, for example light.kitchen."));
  assert.ok(f("Light.Kitchen")[0].includes("Not an entity id"));
  assert.ok(f("fan.bedroom")[0].includes("Unknown domain 'fan'. Supported: cover, light, switch"));
  assert.deepStrictEqual(f("light.kitchen"), []);
  assert.deepStrictEqual(msgs(GOOD.replace("light.kitchen", "person.alice\n        type: sensor"), "error"), []);
});
test("groups", () => {
  const t = (g, wid) => "version: 1\ngroups:\n" + g + "tabs:\n  - title: A\n    widgets:\n" + wid;
  assert.deepStrictEqual(msgs(t("  ok_group: [light.a, light.b]\n", "      - group: ok_group\n        name: G\n"), "error"), []);
  assert.ok(msgs(t("  mixed: [light.a, cover.b]\n", "      - entity: light.a\n"), "error")[0].includes("Group 'mixed' mixes domains."));
  assert.ok(msgs(t("  Bad-Name: [light.a]\n", "      - entity: light.a\n"), "error")[0].includes("Group name 'Bad-Name': lowercase letters, digits and underscore only."));
  assert.ok(msgs(t("  g: [light.a]\n", "      - group: nope\n        name: X\n"), "error")[0].includes("Group 'nope' is not defined."));
  assert.deepStrictEqual(msgs(t("  g:\n    - light.a\n    - light.b\n", "      - group: g\n        name: G\n"), "error"), []);
});
test("warnings do not block", () => {
  const w = msgs("version: 1\nfoo: 1\ntabs:\n  - title: A\n    widgets:\n      - entity: light.a\n        nmae: x\n      - entity: light.a\n  - title: Empty\n    widgets: []\n");
  assert.ok(w.some((m) => m.includes("Unknown key 'foo'. The device ignores it.") && !m.includes("Did you mean")));
  assert.ok(w.some((m) => m.includes("Unknown key 'nmae'") && m.includes("Did you mean 'name'?")));
  assert.ok(w.some((m) => m.includes("Two widgets use 'light.a'.")));
  assert.ok(w.some((m) => m.includes("Tab 'Empty' has no widgets.")));
  assert.deepStrictEqual(msgs("version: 1\nfoo: 1\ntabs:\n  - title: A\n    widgets:\n      - entity: light.a\n", "error"), []);
});
test("size limit", () => {
  assert.ok(msgs(GOOD + "# " + "x".repeat(65 * 1024) + "\n", "error")[0].includes("File is over 64 KB."));
});
test("comments, flow lists and quotes are read", () => {
  const t = "version: 1 # c\nsleep_screen:\n  notifications: true\ntabs:\n  - title: \"A # not a comment\"\n    widgets:\n      - entity: sensor.a\n        details: [humidity, 'dew_point']\n";
  assert.deepStrictEqual(C.checkConfig(t), []);
});

// ---- secrets ----
const good = () => ({ ssid: "Home", password: "12345678", internal_url: "http://192.168.1.10:8123", token: "t".repeat(150), external_url: "", second: { on: false } });
test("secrets validation", () => {
  assert.ok(C.validateSecrets(good()).ok);
  const e = (mut) => { const f = good(); mut(f); return C.validateSecrets(f); };
  assert.strictEqual(e((f) => (f.ssid = "")).errors.ssid, "Required.");
  assert.strictEqual(e((f) => (f.password = "1234567")).errors.password, "Wi-Fi password must be 8 to 63 characters, or empty for an open network.");
  assert.ok(e((f) => (f.password = "")).ok);
  assert.ok(!e((f) => (f.password = "x".repeat(64))).ok);
  assert.strictEqual(e((f) => (f.internal_url = "192.168.1.10")).errors.internal_url, "Must start with http:// or https://");
  assert.strictEqual(e((f) => (f.external_url = "http://x.example")).errors.external_url, "The outside address must use https");
  assert.strictEqual(e((f) => (f.token = "")).errors.token, "Required.");
  const short = e((f) => (f.token = "abc"));
  assert.ok(short.ok && short.warnings.token === "Looks short. Tokens have over 100 characters.");
  const two = (m) => e((f) => (f.second = Object.assign({ on: true, name: "", ssid: "Phone", password: "", ha_url: "auto" }, m)));
  assert.ok(two({}).ok);
  assert.strictEqual(two({ ssid: "" }).errors.second_ssid, "Required.");
  assert.ok(!two({ ssid: "Home" }).ok);
  assert.ok(!two({ ha_url: "external" }).ok);
  assert.ok(!two({ name: "x".repeat(21) }).ok);
});
test("buildSecretsYaml output is the documented shape and escapes values", () => {
  const f = good();
  f.ssid = 'My "Wi-Fi" \\ net'; f.password = "p#ss: word'";
  f.external_url = "https://example.ui.nabu.casa";
  f.second = { on: true, name: "Phone", ssid: "Hotspot", password: "", ha_url: "external" };
  const y = C.buildSecretsYaml(f);
  const doc = C.parseYaml(y);
  const wifi = doc.items[0].value.items;
  assert.strictEqual(wifi.length, 2);
  assert.ok(y.includes('ssid: "My \\"Wi-Fi\\" \\\\ net"'));
  assert.ok(y.includes("ha_url: external") && y.includes("name: \"Phone\""));
  assert.ok(y.includes('external_url: "https://example.ui.nabu.casa"'));
  assert.ok(!/password: ""/.test(y));
  // a value cannot break out of its line
  f.second.on = false; f.ssid = 'a"\nhome_assistant: {}\n'; f.token = "t".repeat(120);
  const y2 = C.buildSecretsYaml(f);
  assert.strictEqual(y2.split("\n").length, 8);
  const one = good(); const y1 = C.buildSecretsYaml(one);
  assert.ok(y1.startsWith("wifi:\n  - ssid: ") && !y1.includes("name:") && !y1.includes("external_url"));
  assert.throws(() => C.buildSecretsYaml(Object.assign(good(), { ssid: "" })));
});

(async () => {
  let failed = 0;
  for (const [n, f] of tests) {
    try { await f(); console.log("ok   " + n); } catch (e) { failed++; console.log("FAIL " + n + "\n" + (e && e.stack)); }
  }
  console.log(failed ? failed + " failed" : "all " + tests.length + " passed");
  process.exit(failed ? 1 : 0);
})();
