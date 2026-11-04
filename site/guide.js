// Copy buttons for code blocks (task 110). Without JS the buttons stay hidden.
document.querySelectorAll(".codebox").forEach(function (box) {
  var btn = box.querySelector(".copy");
  btn.hidden = false;
  btn.addEventListener("click", function () {
    var text = box.querySelector("code").textContent;
    var done = function () { btn.textContent = "Copied"; setTimeout(function () { btn.textContent = "Copy"; }, 1500); };
    if (navigator.clipboard) { navigator.clipboard.writeText(text).then(done); }
  });
});

// Click a picture to see it full size; click, Esc or the close button closes it.
(function () {
  var box = document.createElement("div");
  box.className = "zoom";
  box.hidden = true;
  box.setAttribute("role", "dialog");
  box.setAttribute("aria-label", "Enlarged picture");
  var big = document.createElement("img");
  var close = document.createElement("button");
  close.className = "zoom-close";
  close.textContent = "Close";
  box.appendChild(close);
  box.appendChild(big);
  document.body.appendChild(box);
  var hide = function () { box.hidden = true; big.removeAttribute("src"); };
  box.addEventListener("click", hide);
  document.addEventListener("keydown", function (e) { if (e.key === "Escape") { hide(); } });
  document.querySelectorAll(".guide img.shot").forEach(function (img) {
    img.tabIndex = 0;
    var open = function () { big.src = img.getAttribute("data-zoom") || img.currentSrc || img.src; big.alt = img.alt; box.hidden = false; close.focus(); };
    img.addEventListener("click", open);
    img.addEventListener("keydown", function (e) { if (e.key === "Enter" || e.key === " ") { e.preventDefault(); open(); } });
  });
})();
