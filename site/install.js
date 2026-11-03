// Version picker for /install/ (task 102). Reads releases.json (newest first), wires the install button.
(function () {
  "use strict";
  var $ = function (id) { return document.getElementById(id); };
  if (!("serial" in navigator)) { $("unsupported").hidden = false; return; }
  var releases = [];

  function visible() {
    var withPre = $("pre").checked;
    return releases.filter(function (r) { return withPre || !r.prerelease; });
  }

  function render() {
    var sel = $("ver"), keep = sel.value, list = visible();
    sel.innerHTML = "";
    list.forEach(function (r) {
      var o = document.createElement("option");
      o.value = r.tag;
      o.textContent = r.version + (r.prerelease ? " (pre-release)" : "") + ", " + r.date;
      sel.appendChild(o);
    });
    var stable = list.filter(function (r) { return !r.prerelease; })[0];
    var want = list.some(function (r) { return r.tag === keep; }) ? keep : (stable || list[0] || {}).tag;
    if (want) sel.value = want;
    pick();
  }

  function pick() {
    var tag = $("ver").value;
    var r = releases.filter(function (x) { return x.tag === tag; })[0];
    if (!r) return;
    $("btn").setAttribute("manifest", r.manifest);
    $("info").textContent = "Download size " + (r.size / 1048576).toFixed(1) + " MB.";
    var ni = releases.findIndex(function (x) { return !x.prerelease; });
    $("downgrade").hidden = !(ni >= 0 && releases.indexOf(r) > ni);
  }

  fetch("releases.json").then(function (res) {
    if (!res.ok) throw new Error(res.status);
    return res.json();
  }).then(function (data) {
    releases = data;
    if (!releases.length) throw new Error("empty");
    $("picker").hidden = false;
    $("ver").addEventListener("change", pick);
    $("pre").addEventListener("change", render);
    render();
  }).catch(function () { $("failed").hidden = false; });
})();
