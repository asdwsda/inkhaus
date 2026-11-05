"""Security guard for the config editor JS (task 119): forbidden APIs, scanned after comments are stripped."""
import re

FORBIDDEN = [
    (r"\bfetch\s*\(", "fetch"), (r"XMLHttpRequest", "XMLHttpRequest"), (r"sendBeacon", "sendBeacon"),
    (r"\bWebSocket\b", "WebSocket"), (r"\bEventSource\b", "EventSource"), (r"\bimportScripts\b", "importScripts"),
    (r"new\s+Image\b", "new Image"), (r"localStorage", "localStorage"), (r"sessionStorage", "sessionStorage"),
    (r"indexedDB", "indexedDB"), (r"document\.cookie", "cookie"), (r"\bconsole\b", "console"),
    (r"\blocation\b", "location"), (r"pushState|replaceState", "history state"), (r"\bcaches\b", "Cache API"),
    (r"navigator\.clipboard", "clipboard"), (r"\beval\s*\(|new\s+Function\b", "eval"), (r"postMessage", "postMessage"),
    (r"document\.write", "document.write"), (r"\.src\s*=|\.href\s*=", "dynamic src/href"),
    (r"\bimport\s*\(", "dynamic import"),
    (r"\[\s*[\"'`]\s*(fetch|sendBeacon|localStorage|sessionStorage)\s*[\"'`]\s*\]", "bracket access to a network/storage API"),
    (r"setAttribute\s*\(\s*[\"'`]\s*(src|href)\b", "setAttribute src/href"),
    (r"createElement\s*\(\s*[\"'`]\s*(img|script|iframe|link)\b", "createElement of img/script/iframe/link"),
    (r"\.submit\s*\(", "form submit"), (r"new\s+Worker\b", "new Worker"), (r"new\s+Audio\b", "new Audio"),
    (r"new\s+WebSocket\b", "new WebSocket"), (r"\bwindow\s*\[", "window[...]"), (r"\bglobalThis\b", "globalThis"),
]


def strip_js_comments(src):
    """Remove // and /* */ comments, leaving string and template literals intact (regex literals are not
    parsed; a quote inside one is not expected in the editor code)."""
    out, i, n = [], 0, len(src)
    while i < n:
        c = src[i]
        if c in "\"'`":
            j = i + 1
            while j < n and src[j] != c:
                j += 2 if src[j] == "\\" else 1
            out.append(src[i:j + 1])
            i = j + 1
        elif src.startswith("//", i):
            while i < n and src[i] != "\n":
                i += 1
        elif src.startswith("/*", i):
            j = src.find("*/", i + 2)
            i = n if j < 0 else j + 2
            out.append(" ")
        else:
            out.append(c)
            i += 1
    return "".join(out)


def forbidden_in(code):
    return [name for pat, name in FORBIDDEN if re.search(pat, code)]
