// Version picker for /install/ (task 102). Reads releases.json (newest first), wires the install button.
(function () {
  "use strict";
  var $ = function (id) { return document.getElementById(id); };
  var R = "https://github.com/asdwsda/inkhaus/releases/latest/download/inkhaus-installer-";
  var OS = [["Windows", "windows-x86_64.exe", /Win/], ["macOS", "macos-universal.zip", /Mac/], ["Linux", "linux-x86_64.tar.gz", /Linux|X11/]];
  var all = [], newest = null;

  function fail() { $("nosupport").hidden = true; $("app").hidden = true; $("failed").hidden = false; }

  function apps() {
    var ua = navigator.platform + " " + navigator.userAgent, first = OS.filter(function (o) { return o[2].test(ua); })[0];
    $("apps").innerHTML = OS.map(function (o) {
      return '<a class="btn' + (o === first ? " primary" : "") + '" href="' + R + o[1] + '">' + o[0] + "</a>";
    }).join("");
  }

  function render() {
    var sel = $("ver"), keep = sel.value, withPre = $("pre").checked;
    var list = all.filter(function (r) { return withPre || !r.prerelease; });
    sel.innerHTML = list.map(function (r) {
      return '<option value="' + r.tag + '">' + r.version + (r.prerelease ? " PRE-RELEASE" : r === newest ? " latest stable" : "") + ", " + r.date + "</option>";
    }).join("");
    sel.value = list.some(function (r) { return r.tag === keep; }) ? keep : newest.tag;
    pick();
  }

  function cur() { return all.filter(function (x) { return x.tag === $("ver").value; })[0]; }

  // Safe markdown subset: headings, "-"/"*" lists, `code`. Everything goes through textContent.
  function inline(el, text) {
    text.split(/(`[^`]+`)/).forEach(function (p) {
      if (!p) return;
      if (p.length > 2 && p[0] === "`" && p[p.length - 1] === "`") {
        var c = document.createElement("code"); c.textContent = p.slice(1, -1); el.appendChild(c);
      } else el.appendChild(document.createTextNode(p));
    });
  }
  function notes(box, text) {
    box.textContent = "";
    var ul = null;
    (text || "").split("\n").forEach(function (line) {
      var m;
      if ((m = /^\s*[-*]\s+(.*)$/.exec(line))) {
        if (!ul) { ul = document.createElement("ul"); box.appendChild(ul); }
        var li = document.createElement("li"); inline(li, m[1]); ul.appendChild(li); return;
      }
      ul = null;
      if (!line.trim()) return;
      var h = /^#{1,6}\s+(.*)$/.exec(line), e = document.createElement(h ? "h4" : "p");
      inline(e, h ? h[1] : line.trim());
      box.appendChild(e);
    });
    if (!box.firstChild) { var p = document.createElement("p"); p.textContent = "No release notes."; box.appendChild(p); }
  }
  function changelog() {
    var r = cur();
    if (!r) return;
    $("cltitle").textContent = "Changelog " + r.version;
    notes($("clbody"), r.notes);
    var u = r.url && /^https:\/\/github\.com\//.test(r.url) ? r.url : "https://github.com/asdwsda/inkhaus/releases/tag/" + encodeURIComponent(r.tag);
    $("cllink").href = u;
    var d = $("cl");
    if (d.showModal) d.showModal(); else d.setAttribute("open", "");
  }
  function closeCl() { var d = $("cl"); if (d.close) d.close(); else d.removeAttribute("open"); }

  function pick() {
    var r = cur();
    if (!r) return;
    $("clbtn").disabled = false;
    $("btn").setAttribute("manifest", r.manifest);
    $("go").disabled = false;
    $("go").textContent = "Install " + r.version + (r !== newest && !r.prerelease ? " anyway" : "");
    $("size").textContent = (r.size / 1048576).toFixed(1) + " MB";
    var n = $("note");
    n.hidden = r === newest;
    if (r.prerelease) n.textContent = "Pre-releases may have bugs.";
    else n.textContent = "This is older than the newest version (" + newest.version + "). Settings saved by newer versions may be reset.";
  }

  if (!("serial" in navigator)) { apps(); return; }
  $("nosupport").hidden = true;
  $("app").hidden = false;
  fetch("releases.json").then(function (res) {
    if (!res.ok) throw new Error(res.status);
    return res.json();
  }).then(function (data) {
    all = data;
    newest = all.filter(function (r) { return !r.prerelease; })[0];
    if (!newest) throw new Error("empty");
    $("ver").addEventListener("change", pick);
    $("pre").addEventListener("change", render);
    $("clbtn").addEventListener("click", changelog);
    $("clclose").addEventListener("click", closeCl);
    $("cl").addEventListener("keydown", function (e) { if (e.key === "Escape") closeCl(); });
    render();
  }).catch(fail);
})();
