// Checks for the device config editor on /install/ (task 119). Pure functions, no DOM, no network, no storage.
//   ConfigCheck.checkConfig(text)      -> [{line, severity: "error"|"warning", text}]   (config.yaml, not secret)
//   ConfigCheck.validateSecrets(f)     -> {errors: {field: msg}, warnings: {field: msg}, ok}
//   ConfigCheck.buildSecretsYaml(f)    -> secrets.yaml text
// Rules follow lib/config (the device parser) and docs/guide/09-config-reference.md. The YAML reader here
// handles the block subset a dashboard needs (maps, lists, quoted/plain scalars, one-line or wrapped [..] lists,
// comments); it is a pre-flight check, the device parser stays the authority.
// The secrets functions only compute on the values passed in: they keep nothing and never log.
(function (root) {
  "use strict";
  var DOMAINS = ["cover", "light", "switch", "automation", "climate", "sensor", "binary_sensor", "weather", "vacuum",
    "media_player", "calendar", "alarm_control_panel", "lock"];
  var TYPES = DOMAINS.concat(["garage", "notifications", "energy"]);
  var KEYS = {
    top: ["version", "timezone", "sleep_screen", "groups", "tabs", "connection"],
    sleep_screen: ["weather", "calendar", "notifications"],
    tab: ["title", "widgets", "landscape_columns", "portrait_columns"],
    widget: ["entity", "group", "name", "size", "type", "attribute", "details", "unit", "decimals", "forecast", "art",
      "confirm", "usage", "solar", "display", "history"],
    history: ["hours", "type", "statistic", "period", "min", "max", "source"]
  };
  var MAX_TABS = 16, MAX_WIDGETS = 24, MAX_GROUP = 16, MAX_BYTES = 64 * 1024;

  // ---- minimal YAML reader --------------------------------------------------------------------------------------
  function stripComment(s) {
    var q = null;
    for (var i = 0; i < s.length; i++) {
      var c = s[i];
      if (q) { if (c === "\\" && q === '"') i++; else if (c === q) q = null; }
      else if ((c === '"' || c === "'") && (i === 0 || /[\s\[,:{-]/.test(s[i - 1]))) q = c;
      else if (c === "#" && (i === 0 || /\s/.test(s[i - 1]))) return s.slice(0, i);
    }
    return s;
  }
  function synErr(line, msg) { var e = new Error(msg); e.line = line; e.syntax = true; return e; }

  function scalar(raw, line) {
    var s = raw.trim();
    if (s[0] === '"' || s[0] === "'") {
      var q = s[0], i = 1;
      for (; i < s.length; i++) {
        if (q === '"' && s[i] === "\\") { i++; continue; }
        if (s[i] === q) { if (q === "'" && s[i + 1] === "'") { i++; continue; } break; }
      }
      if (i >= s.length) throw synErr(line, "unclosed quote");
      if (s.slice(i + 1).trim()) throw synErr(line, "text after the closing quote");
      var inner = s.slice(1, i);
      return { type: "scalar", value: q === "'" ? inner.replace(/''/g, "'") : inner.replace(/\\(.)/g, "$1"), quoted: true, line: line };
    }
    return { type: "scalar", value: s, quoted: false, line: line };
  }
  function splitFlow(inner) {
    var parts = [], cur = "", q = null;
    for (var i = 0; i < inner.length; i++) {
      var c = inner[i];
      if (q) { cur += c; if (c === q) q = null; } else if (c === '"' || c === "'") { q = c; cur += c; }
      else if (c === ",") { parts.push(cur); cur = ""; } else cur += c;
    }
    if (q) throw synErr(0, "unclosed quote");
    if (cur.trim() || parts.length) parts.push(cur);
    return parts.filter(function (p) { return p.trim() !== ""; });
  }
  function value(raw, line) {
    var s = raw.trim();
    if (s[0] === "[") {
      if (s[s.length - 1] !== "]") throw synErr(line, "unclosed '['");
      return { type: "seq", line: line, items: splitFlow(s.slice(1, -1)).map(function (p) { return scalar(p, line); }) };
    }
    if (s[0] === "{") {
      if (s[s.length - 1] !== "}") throw synErr(line, "unclosed '{'");
      return { type: "flowmap", line: line, items: [] };
    }
    return scalar(s, line);
  }
  function balanced(s) {
    var d = 0, q = null;
    for (var i = 0; i < s.length; i++) {
      var c = s[i];
      if (q) { if (c === q) q = null; } else if (c === '"' || c === "'") q = c;
      else if (c === "[" || c === "{") d++; else if (c === "]" || c === "}") d--;
    }
    return d <= 0;
  }

  function parseYaml(text) {
    var raw = text.replace(/^﻿/, "").split(/\r\n|\n|\r/), L = [];
    for (var n = 0; n < raw.length; n++) {
      var r = raw[n];
      if (/^\s*\t/.test(r) || /^ *\t/.test(r)) throw synErr(n + 1, "tab character in the indentation");
      var body = stripComment(r);
      if (!body.trim()) continue;
      if (/^(---|\.\.\.)\s*$/.test(body)) continue;
      L.push({ n: n + 1, indent: body.length - body.replace(/^ +/, "").length, s: body.trim() });
    }
    var pos = 0;

    function block(indent) {
      if (pos >= L.length) return { type: "scalar", value: "", line: 0 };
      return /^-(\s|$)/.test(L[pos].s) ? seq(L[pos].indent) : map(L[pos].indent);
    }
    function wrapFlow(l) {
      // join following lines while a [ or { is open
      var s = l.s;
      while (/^[\[{]/.test(s) && !balanced(s) && pos < L.length) s += " " + L[pos++].s;
      return s;
    }
    function after(l, rest, indent, allowSameIndentSeq) {
      // value after "key:" or "-" on line l; nested block on the next lines if rest is empty
      rest = rest.trim();
      if (rest === "|" || rest === ">" || /^[|>][-+0-9]*$/.test(rest)) {
        while (pos < L.length && L[pos].indent > indent) pos++;
        return { type: "scalar", value: "", line: l.n, quoted: false };
      }
      if (rest !== "") {
        if (/^[\[{]/.test(rest)) {
          var full = rest;
          while (!balanced(full) && pos < L.length && L[pos].indent > indent) full += " " + L[pos++].s;
          return value(full, l.n);
        }
        return value(rest, l.n);
      }
      if (pos < L.length && (L[pos].indent > indent || (allowSameIndentSeq && L[pos].indent === indent && /^-(\s|$)/.test(L[pos].s))))
        return block(L[pos].indent);
      return { type: "scalar", value: "", line: l.n, quoted: false };
    }
    function map(indent) {
      var node = { type: "map", items: [], line: L[pos].n }, seen = {};
      while (pos < L.length && L[pos].indent === indent) {
        var l = L[pos];
        if (/^-(\s|$)/.test(l.s)) break;
        var m = /^("(?:[^"\\]|\\.)*"|'(?:[^']|'')*'|[^\s:#][^:]*?)\s*:(?:\s+(.*)|$)/.exec(l.s);
        if (!m) {
          var kk = l.s.replace(/\s.*$/, "");
          var e = synErr(l.n, "Missing ':' after '" + l.s.slice(0, 30) + "'");
          e.missingColon = l.s.slice(0, 30); e.keyText = kk;
          throw e;
        }
        var key = m[1].replace(/^["']|["']$/g, "");
        if (Object.prototype.hasOwnProperty.call(seen, key)) throw synErr(l.n, "duplicate key '" + key + "'");
        seen[key] = true;
        pos++;
        var v = after(l, m[2] || "", indent, true);
        node.items.push({ key: key, value: v, line: l.n });
      }
      if (pos < L.length && L[pos].indent > indent) throw synErr(L[pos].n, "unexpected indentation");
      if (pos < L.length && L[pos].indent < indent && node.items.length === 0) throw synErr(L[pos].n, "bad indentation");
      return node;
    }
    function seq(indent) {
      var node = { type: "seq", items: [], line: L[pos].n };
      while (pos < L.length && L[pos].indent === indent && /^-(\s|$)/.test(L[pos].s)) {
        var l = L[pos], rest = l.s.replace(/^-\s*/, ""), off = l.s.length - rest.length;
        if (rest === "") {
          pos++;
          node.items.push(after(l, "", indent, false));
        } else if (/^("(?:[^"\\]|\\.)*"|'(?:[^']|'')*'|[^\s\[{"'#:-][^:]*?)\s*:(?:\s+|$)/.test(rest) && !/^[\[{]/.test(rest)) {
          // "- key: value": the rest of the line becomes the first line of a mapping at column indent+off
          L[pos] = { n: l.n, indent: indent + off, s: rest };
          node.items.push(map(indent + off));
        } else {
          pos++;
          node.items.push(after(l, rest, indent, false));
        }
      }
      if (pos < L.length && L[pos].indent > indent) throw synErr(L[pos].n, "unexpected indentation");
      return node;
    }
    if (!L.length) return { type: "map", items: [], line: 1 };
    var top = block(L[0].indent);
    if (pos < L.length) throw synErr(L[pos].n, "bad indentation");
    return top;
  }

  // ---- helpers --------------------------------------------------------------------------------------------------
  function get(node, key) {
    if (!node || node.type !== "map") return null;
    for (var i = 0; i < node.items.length; i++) if (node.items[i].key === key) return node.items[i];
    return null;
  }
  function str(item) { return item && item.value.type === "scalar" ? item.value.value : null; }
  function lev(a, b) {
    var d = [], i, j;
    for (i = 0; i <= a.length; i++) { d[i] = [i]; }
    for (j = 1; j <= b.length; j++) d[0][j] = j;
    for (i = 1; i <= a.length; i++) for (j = 1; j <= b.length; j++)
      d[i][j] = Math.min(d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + (a[i - 1] === b[j - 1] ? 0 : 1));
    return d[a.length][b.length];
  }
  function suggest(key, known) {
    var best = null, bd = 3;
    known.forEach(function (k) { var d = lev(key, k); if (d < bd) { bd = d; best = k; } });
    return bd <= 2 ? best : null;
  }
  function entityOk(id) { return /^[a-z_]+\.[a-z0-9_]+$/.test(id); }
  function utf8Len(s) {
    if (typeof TextEncoder !== "undefined") return new TextEncoder().encode(s).length;
    return Buffer.byteLength(s, "utf8");
  }

  function checkConfig(text) {
    var out = [];
    function add(line, severity, t) { out.push({ line: line || 1, severity: severity, text: t }); }
    function warnUnknown(node, known) {
      if (!node || node.type !== "map") return;
      node.items.forEach(function (it) {
        if (known.indexOf(it.key) >= 0) return;
        var s = suggest(it.key, known);
        add(it.line, "warning", "Unknown key '" + it.key + "'. The device ignores it." + (s ? " Did you mean '" + s + "'?" : ""));
      });
    }
    function checkEntity(id, line, anyDomain) {
      if (!entityOk(id)) { add(line, "error", "Not an entity id: '" + id + "'. Use domain.name, for example light.kitchen."); return null; }
      var dom = id.split(".")[0];
      if (!anyDomain && DOMAINS.indexOf(dom) < 0) { add(line, "error", "Unknown domain '" + dom + "'. Supported: " + DOMAINS.join(", ") + "."); return null; }
      return dom;
    }
    if (utf8Len(text) > MAX_BYTES) { add(1, "error", "File is over 64 KB."); return out; }
    var doc;
    try { doc = parseYaml(text); } catch (e) {
      if (!e.syntax) throw e;
      if (e.missingColon !== undefined) {
        var k = e.keyText || e.missingColon;
        add(e.line, "error", "Missing ':' after '" + k + "'. Write it as '" + k + ": value'.");
      } else add(e.line, "error", "YAML syntax error: " + e.message + ".");
      return out;
    }
    if (doc.type !== "map") { add(1, "error", "YAML syntax error: the file must be a mapping of keys."); return out; }
    warnUnknown(doc, KEYS.top);
    var ver = get(doc, "version");
    if (!ver) add(1, "error", "'version' is missing. Add 'version: 1' at the top.");
    else if (str(ver) !== "1") add(ver.line, "error", "'version' must be 1.");
    var ss = get(doc, "sleep_screen");
    if (ss && ss.value.type === "map") warnUnknown(ss.value, KEYS.sleep_screen);

    var groups = {}, g = get(doc, "groups");
    if (g && g.value.type === "map") {
      g.value.items.forEach(function (it) {
        if (!/^[a-z0-9_]+$/.test(it.key)) add(it.line, "error", "Group name '" + it.key + "': lowercase letters, digits and underscore only.");
        var members = it.value.type === "seq" ? it.value.items : [];
        if (it.value.type !== "seq" || !members.length) { add(it.line, "error", "Group '" + it.key + "' must be a list of entity ids."); groups[it.key] = true; return; }
        if (members.length > MAX_GROUP) add(it.line, "error", "Group '" + it.key + "' has too many entities (max " + MAX_GROUP + ").");
        var doms = {};
        members.forEach(function (m) {
          if (m.type !== "scalar") return;
          var d = checkEntity(m.value, m.line);
          if (d) doms[d] = true;
        });
        if (Object.keys(doms).length > 1) add(it.line, "error", "Group '" + it.key + "' mixes domains.");
        groups[it.key] = true;
      });
    }
    var tabs = get(doc, "tabs"), used = {};
    if (!tabs || tabs.value.type !== "seq" || !tabs.value.items.length) {
      add(tabs ? tabs.line : 1, "error", "No tabs. Add at least one tab with widgets.");
    } else {
      if (tabs.value.items.length > MAX_TABS) add(tabs.line, "error", "Too many tabs (max " + MAX_TABS + ").");
      tabs.value.items.forEach(function (tab) {
        if (tab.type !== "map") { add(tab.line, "error", "A tab must be a mapping with 'title' and 'widgets'."); return; }
        warnUnknown(tab, KEYS.tab);
        var title = str(get(tab, "title")) || "(no title)";
        if (!get(tab, "title")) add(tab.line, "error", "A tab needs a 'title'.");
        var w = get(tab, "widgets");
        if (!w || w.value.type !== "seq" || !w.value.items.length) { add(w ? w.line : tab.line, "warning", "Tab '" + title + "' has no widgets."); return; }
        if (w.value.items.length > MAX_WIDGETS) add(w.line, "error", "Too many widgets in this tab (max " + MAX_WIDGETS + ").");
        w.value.items.forEach(function (wd) {
          if (wd.type !== "map") { add(wd.line, "error", "A widget must be a mapping, for example 'entity: light.kitchen'."); return; }
          warnUnknown(wd, KEYS.widget);
          var type = str(get(wd, "type")), ent = get(wd, "entity"), grp = get(wd, "group");
          if (type && TYPES.indexOf(type) < 0) add(get(wd, "type").line, "error", "Unknown widget type '" + type + "'.");
          var h = get(wd, "history");
          if (h && h.value.type === "map") warnUnknown(h.value, KEYS.history);
          if (type === "energy") {
            ["usage", "solar"].forEach(function (k) {
              var it = get(wd, k);
              if (it && str(it) !== null) { var d = checkEntity(str(it), it.line); if (d && d !== "sensor") add(it.line, "error", "'" + k + "' must be a sensor entity id."); }
            });
            if (!get(wd, "usage") && !get(wd, "solar")) add(wd.line, "error", "An energy widget needs 'usage' or 'solar'.");
            return;
          }
          if (type === "notifications") return;
          if (ent && grp) { add(wd.line, "error", "Use either 'entity' or 'group' in a widget, not both."); return; }
          if (ent) {
            var id = str(ent);
            if (id === null) { add(ent.line, "error", "Not an entity id: ''. Use domain.name, for example light.kitchen."); return; }
            checkEntity(id, ent.line, type && TYPES.indexOf(type) >= 0);  // `type:` lets any domain through
            if (used[id]) add(ent.line, "warning", "Two widgets use '" + id + "'."); else used[id] = true;
          } else if (grp) {
            var gn = str(grp);
            if (gn === null || !groups[gn]) add(grp.line, "error", "Group '" + (gn || "") + "' is not defined.");
            if (!get(wd, "name")) add(grp.line, "error", "A group widget needs a 'name'.");
          } else add(wd.line, "error", "A widget needs 'entity' or 'group'.");
        });
      });
    }
    out.sort(function (a, b) { return a.line - b.line || (a.severity === "error" ? -1 : 1); });
    return out;
  }

  // ---- secrets --------------------------------------------------------------------------------------------------
  var URL_RE = /^https?:\/\/\S+$/i;
  function validateSecrets(f) {
    var errors = {}, warnings = {};
    function req(k, v, label) { if (!v || !String(v).trim()) errors[k] = "Required."; }
    function pw(k, v) { if (v && (v.length < 8 || v.length > 63)) errors[k] = "Wi-Fi password must be 8 to 63 characters, or empty for an open network."; }
    f = f || {};
    req("ssid", f.ssid);
    pw("password", f.password || "");
    req("internal_url", f.internal_url);
    if (f.internal_url && !URL_RE.test(f.internal_url.trim())) errors.internal_url = "Must start with http:// or https://";
    req("token", f.token);
    if (f.token && !errors.token && f.token.trim().length < 100) warnings.token = "Looks short. Tokens have over 100 characters.";
    var ext = (f.external_url || "").trim();
    if (ext && !/^https:\/\/\S+$/i.test(ext)) errors.external_url = "The outside address must use https";
    if (f.second && f.second.on) {
      var s = f.second;
      req("second_ssid", s.ssid);
      pw("second_password", s.password || "");
      if (s.name && s.name.length > 20) errors.second_name = "At most 20 characters.";
      if (s.name && s.name.trim() === "Network 1") errors.second_name = "This name is already used.";
      if (s.ssid && f.ssid && s.ssid === f.ssid) errors.second_ssid = "Each network needs a different name (SSID).";
      if (s.ha_url === "external" && !ext) errors.second_ha_url = "Needs the outside address above.";
    }
    var bad = Object.keys(errors).length > 0;
    return { errors: errors, warnings: warnings, ok: !bad };
  }
  function q(v) { return JSON.stringify(String(v)); }  // JSON strings are valid YAML double-quoted scalars
  function buildSecretsYaml(f) {
    var v = validateSecrets(f);
    if (!v.ok) throw new Error("secrets form is not valid");
    var o = "wifi:\n";
    var s = f.second && f.second.on ? f.second : null;
    o += "  - ssid: " + q(f.ssid) + "\n";
    if (f.password) o += "    password: " + q(f.password) + "\n";
    if (s) {
      o += "  - name: " + q(s.name && s.name.trim() ? s.name.trim() : "Network 2") + "\n";
      o += "    ssid: " + q(s.ssid) + "\n";
      if (s.password) o += "    password: " + q(s.password) + "\n";
      o += "    ha_url: " + (["auto", "internal", "external"].indexOf(s.ha_url) >= 0 ? s.ha_url : "auto") + "\n";
    }
    o += "home_assistant:\n";
    o += "  internal_url: " + q(f.internal_url.trim()) + "\n";
    if ((f.external_url || "").trim()) o += "  external_url: " + q(f.external_url.trim()) + "\n";
    o += "  token: " + q(f.token.trim()) + "\n";
    return o;
  }

  var api = { checkConfig: checkConfig, validateSecrets: validateSecrets, buildSecretsYaml: buildSecretsYaml, parseYaml: parseYaml,
    DOMAINS: DOMAINS, KEYS: KEYS };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.ConfigCheck = api;
})(typeof self !== "undefined" ? self : this);
