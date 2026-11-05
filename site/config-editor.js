// Device config editor on /install/ (task 119, docs/ui/config-editor.md): reads and writes config.yaml, and
// optionally secrets.yaml, over Web Serial with the provisioning protocol (provision-client.js).
// Security rules (checked by site/check.py): no network request, no storage, no logging, no URL change in
// this file. Secrets live in the form fields only; they are cleared after a write, on "Clear fields", on
// pagehide and when the connection closes. They are never read back from the device.
(function () {
  "use strict";
  if (!("serial" in navigator)) return;
  var PC = window.ProvisionClient, CC = window.ConfigCheck;
  var $ = function (id) { return document.getElementById(id); };
  $("config").hidden = false;

  var lastRead = null;      // text as read from the device (null until a read worked)
  var busy = false, locked = false;
  var session = null;       // {port, reader, writer, client}
  var checkTimer = null, msgs = [];
  var touched = {};

  // ---- status strip ---------------------------------------------------------------------------------------------
  var GLYPH = { ok: "✓", busy: "◐", err: "▲", idle: "" };
  function strip(ids, kind, head, detail) {
    var el = $(ids.strip);
    el.hidden = false;
    el.className = "strip" + (kind === "err" ? " err" : "");
    $(ids.g).textContent = GLYPH[kind] || "";
    $(ids.h).textContent = head;
    $(ids.d).textContent = detail || "";
  }
  var CFG = { strip: "strip", g: "sg", h: "sh", d: "sd" }, SEC = { strip: "secstrip", g: "secg", h: "sech", d: "secd" };
  function progress(done, total, label) {
    var b = $("bar"), c = $("barcap");
    if (done === null) { b.hidden = true; c.hidden = true; return; }
    b.hidden = false; c.hidden = false;
    $("barfill").style.width = Math.round(100 * done / total) + "%";
    c.textContent = done + " of " + total + " bytes" + (label ? ", " + label : "");
  }
  var ERRS = {
    port: ["No port chosen or port busy", "Close serial monitors and the installer app, then try again."],
    "no-answer": ["The M5Paper did not answer", "Press the rocker once to wake it. It needs Inkhaus 1.5 or newer."],
    unplugged: ["Cable unplugged during the transfer", "Plug it in and try again. The old file on the device is intact."],
    timeout: ["Cable unplugged during the transfer", "Plug it in and try again. The old file on the device is intact."],
    refused: ["The device refused the file", "Checksum or size (over 64 KB) was wrong. Try again."],
    crc: ["The device refused the file", "Checksum or size (over 64 KB) was wrong. Try again."],
    "no-file": ["No config.yaml on the device", "The SD card has no config.yaml yet. Click \u201cStart from the example\u201d, adjust it, then write it to the device."]
  };
  function fail(ids, e) {
    var m = ERRS[e && e.code] || ERRS.port;
    strip(ids, "err", m[0], m[1]);
  }

  // ---- connection -----------------------------------------------------------------------------------------------
  function makeTransport(port) {
    var reader = port.readable.getReader(), writer = port.writable.getWriter();
    return {
      reader: reader, writer: writer,
      write: function (u8) { return writer.write(u8); },
      read: function () { return reader.read().then(function (r) { return r.done ? null : r.value; }); }
    };
  }
  async function closeSession() {
    var s = session;
    session = null;
    if (!s) return;
    try { await s.t.reader.cancel(); } catch (e) { /* closed */ }
    try { s.t.reader.releaseLock(); } catch (e) { /* closed */ }
    try { s.t.writer.releaseLock(); } catch (e) { /* closed */ }
    try { await s.port.close(); } catch (e) { /* closed */ }
    clearSecrets();
  }
  // Opens the port (user gesture needed for the first call) and wakes the board.
  async function connect(ids) {
    if (session) return session.client;
    strip(ids, "busy", "Waiting for the USB port", "Pick the M5Paper in the browser window.");
    var port;
    try { port = await navigator.serial.requestPort(); } catch (e) { return null; }  // cancelled: back to idle
    try { await port.open({ baudRate: 115200 }); } catch (e) { fail(ids, { code: "port" }); return undefined; }
    var t = makeTransport(port), client = new PC.Client(t);
    session = { port: port, t: t, client: client };
    port.addEventListener("disconnect", function () { if (session && session.port === port) { session = null; clearSecrets(); } });
    strip(ids, "busy", "Connecting", "Waking the M5Paper. This can take up to 30 seconds.");
    await client.wake(function (dtr, rts) { return port.setSignals({ dataTerminalReady: dtr, requestToSend: rts }); });
    return client;
  }

  // ---- config editor --------------------------------------------------------------------------------------------
  function setBusy(b) { busy = b; refresh(); }
  function refresh() {
    var has = lastRead !== null, errs = msgs.filter(function (m) { return m.severity === "error"; }).length;
    $("rd").disabled = busy || locked;
    $("ex").disabled = busy;
    $("wr").disabled = !has || errs > 0 || busy || locked;
    $("dis").disabled = !has || busy || $("txt").value === lastRead;
    $("txt").disabled = !has || busy;
    $("ed").className = "editor" + (has ? "" : " off");
    $("secwr").disabled = busy || locked || !CC.validateSecrets(secretsValues()).ok;
  }
  function bytesOf(text) { return new TextEncoder().encode(text); }

  async function doRead() {
    if (busy) return;
    setBusy(true);
    try {
      var client = await connect(CFG);
      if (!client) { if (client === null) strip(CFG, "idle", "Not connected", "Click the button, then pick the M5Paper in the browser window."); return; }
      strip(CFG, "busy", "Reading config.yaml", "Do not unplug the cable.");
      progress(0, 1);
      var r = await client.getFile("config.yaml", function (a, b) { progress(a, b); });
      progress(null);
      lastRead = new TextDecoder().decode(r.bytes);
      $("txt").value = lastRead;
      var now = new Date();
      strip(CFG, "ok", "Read config.yaml, " + r.bytes.length + " bytes",
        "From the M5Paper at " + now.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" }) + ". Your edits stay on this page until you write.");
      runCheck();
    } catch (e) {
      progress(null);
      fail(CFG, e);
      await closeSession();
    } finally { setBusy(false); }
  }

  async function doWrite() {
    if (busy || lastRead === null) return;
    runCheck();
    if (msgs.some(function (m) { return m.severity === "error"; })) return;
    setBusy(true);
    try {
      var client = await connect(CFG);
      if (!client) { if (client === null) strip(CFG, "idle", "Not connected", "Click the button, then pick the M5Paper in the browser window."); return; }
      var text = $("txt").value, bytes = bytesOf(text);
      strip(CFG, "busy", "Writing config.yaml", "Do not unplug the cable.");
      progress(0, bytes.length, "checking on the device");
      var r = await client.putFile("config.yaml", bytes, function (a, b) { progress(a, b, "checking on the device"); });
      progress(null);
      lastRead = text;
      await client.reboot();
      strip(CFG, "ok", "config.yaml written and checked", "The M5Paper restarts now (about 10 s)." + (/error/.test(r.summary) ? " The device reports a problem with the file: " + r.summary.replace(/^PUT-CONFIG /, "") : ""));
      await closeSession();
      lockFor(10000);
    } catch (e) {
      progress(null);
      fail(CFG, e);
      await closeSession();
    } finally { setBusy(false); }
  }
  function lockFor(ms) { locked = true; refresh(); setTimeout(function () { locked = false; refresh(); }, ms); }

  // ---- check list -----------------------------------------------------------------------------------------------
  function runCheck() {
    clearTimeout(checkTimer);
    if (lastRead === null) { $("chkh").textContent = ""; $("chkl").textContent = ""; msgs = []; refresh(); return; }
    msgs = CC.checkConfig($("txt").value);
    renderGutter();
    var e = msgs.filter(function (m) { return m.severity === "error"; }).length, w = msgs.length - e;
    $("chkh").textContent = msgs.length ? "Check: " + e + " error" + (e === 1 ? "" : "s") + ", " + w + " warning" + (w === 1 ? "" : "s") : "Check: OK ✓";
    var ul = $("chkl");
    ul.textContent = "";
    msgs.slice(0, 8).forEach(function (m) {
      var li = document.createElement("li"), b = document.createElement("button");
      b.type = "button";
      var mk = document.createElement("span"); mk.className = "mk" + (m.severity === "error" ? " e" : ""); mk.textContent = m.severity === "error" ? "!" : "?";
      var ln = document.createElement("span"); ln.className = "ln"; ln.textContent = "Line " + m.line;
      var tx = document.createElement("span"); tx.textContent = m.text;
      b.appendChild(mk); b.appendChild(ln); b.appendChild(tx);
      b.addEventListener("click", function () { gotoLine(m.line); });
      li.appendChild(b); ul.appendChild(li);
    });
    if (msgs.length > 8) { var li = document.createElement("li"); li.className = "more"; li.textContent = "and " + (msgs.length - 8) + " more"; ul.appendChild(li); }
    refresh();
  }
  function renderGutter() {
    var n = $("txt").value.split("\n").length, mark = {};
    msgs.forEach(function (m) { if (mark[m.line] !== "e") mark[m.line] = m.severity === "error" ? "e" : "w"; });
    var g = $("gut");
    g.textContent = "";
    var frag = document.createDocumentFragment();
    for (var i = 1; i <= n; i++) {
      var s = document.createElement("span");
      if (mark[i]) s.className = mark[i];
      s.textContent = String(i);
      frag.appendChild(s);
    }
    g.appendChild(frag);
    g.scrollTop = $("txt").scrollTop;
  }
  function gotoLine(n) {
    var t = $("txt"), lines = t.value.split("\n"), pos = 0;
    for (var i = 0; i < n - 1 && i < lines.length; i++) pos += lines[i].length + 1;
    t.focus();
    t.setSelectionRange(pos, pos);
    t.scrollTop = Math.max(0, (n - 3) * 24);
    $("gut").scrollTop = t.scrollTop;
  }
  $("txt").addEventListener("input", function () { clearTimeout(checkTimer); checkTimer = setTimeout(runCheck, 300); refresh(); });
  $("txt").addEventListener("scroll", function () { $("gut").scrollTop = $("txt").scrollTop; });
  $("rd").addEventListener("click", doRead);
  $("wr").addEventListener("click", doWrite);
  // Start from the built-in example (inlined at build time): for a device with no config.yaml yet.
  $("ex").addEventListener("click", function () {
    if (busy) return;
    if (lastRead !== null && $("txt").value !== lastRead && !window.confirm("Replace your edits with the example?")) return;
    lastRead = "";
    $("txt").value = $("exampletxt").textContent.replace(/^\n/, "");
    strip(CFG, "ok", "Example loaded", "This is the built-in example, not the file on the device. Edit it, then write it to the device.");
    runCheck();
  });
  $("dis").addEventListener("click", function () { $("txt").value = lastRead; runCheck(); });

  // ---- secrets (write-only) -------------------------------------------------------------------------------------
  function val(id) { return $(id).value; }
  function secretsValues() {
    var r = document.querySelector('input[name="hauri"]:checked');
    return {
      ssid: val("f_ssid"), password: val("f_password"), internal_url: val("f_internal_url"), token: val("f_token"),
      external_url: val("f_external_url"),
      second: { on: $("sec2").checked, name: val("f_second_name"), ssid: val("f_second_ssid"), password: val("f_second_password"), ha_url: r ? r.value : "auto" }
    };
  }
  function renderSecretsMsgs(all) {
    var v = CC.validateSecrets(secretsValues());
    document.querySelectorAll("#sf input[data-f]").forEach(function (inp) {
      var k = inp.getAttribute("data-f"), m = $("m_" + k);
      var show = all || touched[k], e = show && v.errors[k], w = show && !e && v.warnings[k];
      m.textContent = e ? "! " + e : w ? "? " + w : "";
      m.className = "fmsg" + (w ? " w" : "");
      inp.className = e ? "bad" : "";
      if (e) inp.setAttribute("aria-invalid", "true"); else inp.removeAttribute("aria-invalid");
    });
    var ext = !val("f_external_url").trim() || !!v.errors.external_url;
    $("r_ext").disabled = ext;
    $("exthint").hidden = !ext;
    if (ext && $("r_ext").checked) document.querySelector('input[name="hauri"][value="auto"]').checked = true;
    var m2 = $("m_second_ha_url");
    m2.textContent = (all || touched.second_ha_url) && v.errors.second_ha_url ? "! " + v.errors.second_ha_url : "";
    refresh();
    return v;
  }
  function clearSecrets() {
    document.querySelectorAll("#sf input").forEach(function (i) {
      if (i.type === "checkbox" || i.type === "radio") return;
      i.value = "";
    });
    touched = {};
    $("showch").checked = false;
    applyMask();
    if ($("sf")) renderSecretsMsgs(false);
  }
  function applyMask() {
    var show = $("showch").checked;
    ["f_password", "f_token", "f_second_password"].forEach(function (id) { $(id).type = show ? "text" : "password"; });
  }
  async function doWriteSecrets() {
    var v = renderSecretsMsgs(true);
    if (!v.ok || busy) return;
    setBusy(true);
    var bytes = null;
    try {
      var client = await connect(SEC);
      if (!client) { if (client === null) $("secstrip").hidden = true; return; }
      bytes = bytesOf(CC.buildSecretsYaml(secretsValues()));
      strip(SEC, "busy", "Writing secrets.yaml", "Do not unplug the cable.");
      var r = await client.putFile("secrets.yaml", bytes);
      if (/error/.test(r.summary)) { strip(SEC, "err", "The device refused the secrets", "Check the fields and try again."); return; }
      await client.reboot();
      clearSecrets();
      strip(SEC, "ok", "secrets.yaml written", "The fields were cleared. The M5Paper restarts now (about 10 s).");
      await closeSession();
      lockFor(10000);
    } catch (e) {
      fail(SEC, e);
      await closeSession();
    } finally {
      if (bytes) bytes.fill(0);
      setBusy(false);
    }
  }
  $("secopen").addEventListener("click", function () {
    var f = $("secform"), open = f.hidden;
    f.hidden = !open;
    $("secopen").setAttribute("aria-expanded", open ? "true" : "false");
    $("secopen").textContent = open ? "Hide secrets form" : "Set up secrets...";
  });
  document.querySelectorAll("#sf input[data-f]").forEach(function (inp) {
    inp.addEventListener("input", function () { renderSecretsMsgs(false); });
    inp.addEventListener("blur", function () { touched[inp.getAttribute("data-f")] = true; renderSecretsMsgs(false); });
  });
  $("showch").addEventListener("change", applyMask);
  $("sec2").addEventListener("change", function () { $("sec2box").hidden = !$("sec2").checked; renderSecretsMsgs(false); });
  document.querySelectorAll('input[name="hauri"]').forEach(function (r) { r.addEventListener("change", function () { renderSecretsMsgs(false); }); });
  $("secwr").addEventListener("click", doWriteSecrets);
  $("secclr").addEventListener("click", function () { clearSecrets(); $("secstrip").hidden = true; });

  // ---- page lifecycle -------------------------------------------------------------------------------------------
  window.addEventListener("pagehide", function () { clearSecrets(); closeSession(); });
  window.addEventListener("beforeunload", function (e) {
    if (lastRead !== null && $("txt").value !== lastRead) { e.preventDefault(); e.returnValue = ""; }  // browser's own confirm, no text
  });
  // The installer needs the port: let go of it when the user starts an install.
  var go = $("go");
  if (go) go.addEventListener("click", function () { closeSession(); }, true);
  renderSecretsMsgs(false);
  refresh();
})();
