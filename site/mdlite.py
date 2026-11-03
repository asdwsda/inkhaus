"""Small stdlib-only Markdown renderer for the user guide (task 110).
Supports: # headings (with anchors), paragraphs, bullet and numbered lists (flat), fenced code,
pipe tables, blockquotes, images (figure; the title is the caption), links, **bold**, *italic*, `code`.
Raw HTML in the source is escaped."""
import html, re


def slug(text):
    s = re.sub(r"[^a-z0-9]+", "-", re.sub(r"<[^>]+>", "", text).lower()).strip("-")
    return s or "section"


def inline(t):
    codes = []

    def stash(m):
        codes.append(f"<code>{html.escape(m.group(1))}</code>")
        return f"\x00{len(codes) - 1}\x00"

    t = re.sub(r"`([^`]+)`", stash, t)
    t = html.escape(t, quote=False)
    t = re.sub(r"!\[([^\]]*)\]\(([^)\s]+)(?:\s+&quot;[^)]*&quot;|\s+\"[^)]*\")?\)",
               lambda m: f'<img src="{m.group(2)}" alt="{html.escape(m.group(1))}">', t)
    t = re.sub(r"\[([^\]]+)\]\(([^)\s]+)\)", lambda m: f'<a href="{html.escape(m.group(2))}">{m.group(1)}</a>', t)
    t = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", t)
    t = re.sub(r"(?<![\w*])\*([^*\s][^*]*)\*(?![\w*])", r"<em>\1</em>", t)
    return re.sub(r"\x00(\d+)\x00", lambda m: codes[int(m.group(1))], t)


def render(md):
    """Return (html, headings) where headings = [(level, id, text)]."""
    lines = md.splitlines()
    out, heads, used = [], [], set()
    i = 0
    while i < len(lines):
        ln = lines[i]
        if not ln.strip():
            i += 1
            continue
        m = re.match(r"```(\w*)\s*$", ln)
        if m:
            j = i + 1
            while j < len(lines) and not lines[j].startswith("```"):
                j += 1
            code = html.escape("\n".join(lines[i + 1:j]))
            out.append(f'<div class="codebox"><button class="copy" type="button" hidden aria-label="Copy code">Copy</button>'
                       f'<pre tabindex="0"><code>{code}</code></pre></div>')
            i = j + 1
            continue
        m = re.match(r"(#{1,4})\s+(.*?)\s*$", ln)
        if m:
            lvl, text = len(m.group(1)), m.group(2)
            base = slug(text)
            hid, n = base, 2
            while hid in used:
                hid, n = f"{base}-{n}", n + 1
            used.add(hid)
            heads.append((lvl, hid, re.sub(r"[`*]", "", text)))
            out.append(f'<h{lvl} id="{hid}">{inline(text)}<a class="anchor" href="#{hid}" aria-label="Link to this section">#</a></h{lvl}>')
            i += 1
            continue
        m = re.match(r'!\[([^\]]*)\]\(([^)\s]+)(?:\s+"([^"]*)")?\)\s*$', ln)
        if m:
            alt, src, cap = m.group(1), m.group(2), m.group(3)
            fig = f'<figure><img class="shot" src="{html.escape(src)}" alt="{html.escape(alt)}">'
            if cap:
                fig += f'<figcaption class="cap">{inline(cap)}</figcaption>'
            out.append(fig + "</figure>")
            i += 1
            continue
        if ln.startswith("|") and i + 1 < len(lines) and re.match(r"\|[\s:|-]+\|\s*$", lines[i + 1]):
            cells = lambda s: [c.strip() for c in s.strip().strip("|").split("|")]
            head = cells(ln)
            j = i + 2
            rows = []
            while j < len(lines) and lines[j].startswith("|"):
                rows.append(cells(lines[j]))
                j += 1
            t = "<div class=\"tablewrap\"><table><thead><tr>" + "".join(f"<th>{inline(c)}</th>" for c in head) + "</tr></thead><tbody>"
            for r in rows:
                t += "<tr>" + "".join(f"<td>{inline(c)}</td>" for c in r) + "</tr>"
            out.append(t + "</tbody></table></div>")
            i = j
            continue
        m = re.match(r"([-*]|\d+\.)\s+", ln)
        if m:
            tag = "ol" if m.group(1)[0].isdigit() else "ul"
            items = []
            while i < len(lines) and re.match(r"([-*]|\d+\.)\s+", lines[i]):
                items.append(re.sub(r"^([-*]|\d+\.)\s+", "", lines[i]))
                i += 1
                while i < len(lines) and lines[i].startswith("  ") and lines[i].strip():
                    items[-1] += " " + lines[i].strip()
                    i += 1
            out.append(f"<{tag}>" + "".join(f"<li>{inline(x)}</li>" for x in items) + f"</{tag}>")
            continue
        if ln.startswith(">"):
            buf = []
            while i < len(lines) and lines[i].startswith(">"):
                buf.append(lines[i].lstrip("> ").rstrip())
                i += 1
            out.append(f'<p class="note">{inline(" ".join(buf))}</p>')
            continue
        buf = []
        while i < len(lines) and lines[i].strip() and not re.match(r"(```|#{1,4}\s|>|\||([-*]|\d+\.)\s)", lines[i]):
            buf.append(lines[i].strip())
            i += 1
        if not buf:  # stray line that matched a block start but not its form
            buf.append(lines[i].strip())
            i += 1
        out.append(f"<p>{inline(' '.join(buf))}</p>")
    return "\n".join(out), heads
