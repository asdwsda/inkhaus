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
