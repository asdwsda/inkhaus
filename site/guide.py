"""Build the user guide (task 110): docs/guide/NN-name.md -> dist/guide/name/index.html plus dist/guide/index.html.
Page order comes from the NN- filename prefix; the title is the first '# ' heading."""
import html, re, shutil
import mdlite

PREFIX = re.compile(r"^\d+-")


def pages(src):
    out = []
    for f in sorted(src.glob("*.md")):
        md = f.read_text()
        m = re.match(r"# (.+)", md)
        if not m:
            raise SystemExit(f"{f}: first line must be '# Title'")
        out.append({"slug": PREFIX.sub("", f.stem), "title": m.group(1).strip(), "md": md})
    return out


def fix_links(body, slugs, imgdir=None):
    """img/x.png -> ../img/x.png ; NN-name.md#a -> ../name/#a ; adds data-zoom for pictures with an img/2x/ version"""
    def zoom(m):
        name = m.group(2)
        if imgdir is not None and (imgdir / "2x" / name).is_file():
            return f'{m.group(1)}data-zoom="../img/2x/{name}" src="img/{name}"'
        return m.group(0)
    body = re.sub(r'(<img class="shot" )src="img/([^"/]+)"', zoom, body)
    body = re.sub(r'(<img [^>]*src=")img/', r"\1../img/", body)

    def link(m):
        if m.group(1) not in slugs:
            raise SystemExit(f"guide: link to unknown page {m.group(1)}.md")
        return f'href="../{m.group(1)}/{m.group(2) or ""}"'
    return re.sub(r'href="(?:\d+-)?([a-z0-9-]+)\.md(#[^"]*)?"', link, body)


def shell(title, base, nav_html, body, toc_html=""):
    return f'''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{html.escape(title)} - Inkhaus user guide</title>
<link rel="icon" href="{base}logo-inkhaus.svg" type="image/svg+xml">
<link rel="stylesheet" href="{base}style.css">
</head>
<body>
<header><div class="wrap">
<a class="brand" href="{base}"><img src="{base}logo-inkhaus.svg" width="48" height="48" alt="">Inkhaus</a>
<nav aria-label="Main"><a href="{base}">Home</a><a href="{base}guide/">User guide</a><a href="{base}install/">Install</a><a href="https://github.com/asdwsda/inkhaus">GitHub</a></nav>
</div></header>
<div class="wrap guide">
<aside class="sidebar"><nav aria-label="Guide">{nav_html}</nav></aside>
<main>
{body}
{toc_html}
</main>
</div>
<footer><div class="wrap">
<p>Inkhaus, open source on <a href="https://github.com/asdwsda/inkhaus">GitHub</a>.</p>
</div></footer>
<script src="{base}guide/guide.js"></script>
</body>
</html>
'''


def build(root, out, site):
    src = root / "docs/guide"
    ps = pages(src)
    slugs = {p["slug"] for p in ps}
    gout = out / "guide"
    gout.mkdir(parents=True)
    shutil.copy(site / "guide.js", gout / "guide.js")
    if (src / "img").is_dir():
        shutil.copytree(src / "img", gout / "img")

    def nav(prefix, current):
        items = ""
        for p in ps:
            cur = ' aria-current="page"' if p["slug"] == current else ""
            items += f'<li><a href="{prefix}{p["slug"]}/"{cur}>{html.escape(p["title"])}</a></li>'
        return f"<ul>{items}</ul>"

    for i, p in enumerate(ps):
        body, heads = mdlite.render(p["md"])
        body = fix_links(body, slugs, src / "img")
        sub = [h for h in heads if h[0] == 2]
        toc = ""
        if len(sub) > 1:
            toc = '<nav class="toc" aria-label="On this page"><strong>On this page</strong><ul>' + "".join(
                f'<li><a href="#{hid}">{html.escape(t)}</a></li>' for _, hid, t in sub) + "</ul></nav>"
        pager = '<p class="pager">'
        pager += f'<a href="../{ps[i-1]["slug"]}/">&larr; {html.escape(ps[i-1]["title"])}</a>' if i else "<span></span>"
        if i + 1 < len(ps):
            pager += f'<a href="../{ps[i+1]["slug"]}/">{html.escape(ps[i+1]["title"])} &rarr;</a>'
        pager += "</p>"
        d = gout / p["slug"]
        d.mkdir()
        (d / "index.html").write_text(shell(p["title"], "../../", nav("../", p["slug"]), toc + body + pager))
    lst = "<ol>" + "".join(f'<li><a href="{p["slug"]}/">{html.escape(p["title"])}</a></li>' for p in ps) + "</ol>"
    body = f"<h1>User guide</h1>\n<p>How to set up and use Inkhaus.</p>\n{lst}"
    (gout / "index.html").write_text(shell("User guide", "../", nav("", ""), body))
    return len(ps)
