// Writes test/test_provision/secrets_roundtrip.txt: buildSecretsYaml output for tricky passwords, one record per line:
//   <ssid hex> <password hex> <yaml hex>
// test/test_provision parses the yaml with config::parseSecrets and expects ssid/password back.
// Usage: node site/test/gen-secrets-fixture.js   (check.py regenerates it and fails when the committed copy is stale)
"use strict";
const fs = require("fs");
const path = require("path");
const C = require("../config-check.js");
const PW = ["p\"q\\r#x: y\nz", "it's #1 - [a]{b}", "tab\there", "ünï 日本 \u{1F600}", "a: b", "- x", "&anchor *alias !tag",
  "'single'", "\"double\"", "  lead and trail  ", "|>%@`", "{\"k\": 1}", "null", "~", "true", "0x1F", "1e3", "back\\slash\\", "x # not a comment",
  "---", "...", "\r\n", "\u0001ctl", " sep", "a".repeat(63)];
const rows = PW.map((pw0, i) => {
  const pw = pw0.length >= 8 ? pw0 : pw0 + "_pad".repeat(3).slice(0, 8 - pw0.length + 1);
  const ssid = "net" + i + " #x";
  const y = C.buildSecretsYaml({ ssid: ssid, password: pw, internal_url: "http://192.168.1.10:8123", external_url: "", token: "t.o-k_" + i });
  const h = (s) => Buffer.from(s, "utf8").toString("hex");
  return h(ssid) + " " + h(pw) + " " + h(y);
});
const out = path.join(__dirname, "..", "..", "test", "test_provision", "secrets_roundtrip.txt");
fs.writeFileSync(out, rows.join("\n") + "\n");
