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

  function pick() {
    var r = all.filter(function (x) { return x.tag === $("ver").value; })[0];
    if (!r) return;
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
    render();
  }).catch(fail);
})();
