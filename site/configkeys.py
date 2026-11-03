"""Config key lists (task 110): extracted from lib/config/src/config.cpp, config.example.yaml and the guide
reference page, never hand-kept. Used by check.py."""
import re
from pathlib import Path

root = Path(__file__).resolve().parent.parent


def code_keys():
    """Keys the parser reads: str(o, "k"...), ["k"], strcmp(k, "k"), {"k", &field} tables, known[] lists."""
    src = (root / "lib/config/src/config.cpp").read_text()
    keys = set(re.findall(r'\b(?:str|num|boolean|getStr)\(\w+,\s*"([a-z_]+)"', src))
    keys |= set(re.findall(r'\["([a-z_]+)"\]', src))
    keys |= set(re.findall(r'strcmp\(\w+,\s*"([a-z_]+)"\)', src))
    keys |= set(re.findall(r'\{"([a-z_]+)",\s*&', src))
    keys |= set(re.findall(r'\{"[a-z_]+",\s*"([a-z_]+)",\s*&', src))
    for blk in re.findall(r"known\[\]\s*=\s*\{([^}]*)\}", src):
        keys |= set(re.findall(r'"([a-z_]+)"', blk))
    return keys


def yaml_keys(name, skip_groups=False):
    """Every `key:` in an example YAML, commented-out examples included. Group names under `groups:`
    (indent 2, until the next top-level line) are user names, not keys."""
    keys, in_groups = set(), False
    for ln in (root / name).read_text().splitlines():
        if re.match(r"groups:", ln):
            in_groups = True
        elif re.match(r"\S", ln) and not ln.startswith("#"):
            in_groups = False
        m = re.match(r"(\s*)(?:#\s?)?(?:-\s+)?([a-z_]+):(\s|$)", ln)
        if m:
            if skip_groups and in_groups and len(m.group(1)) == 2:
                continue
            keys.add(m.group(2))
    return keys


def reference_keys(md_path):
    """First-column `key` cells of the tables in the config reference page, by section:
    {"config.yaml": {...}, "secrets.yaml": {...}} (section = nearest preceding H2)."""
    out, sec = {}, None
    for ln in md_path.read_text().splitlines():
        m = re.match(r"##\s+(\S+)", ln)
        if m:
            sec = m.group(1)
        elif sec and ln.startswith("|") and not re.match(r"\|[\s:|-]+\|", ln):
            first = ln.strip().strip("|").split("|")[0]
            out.setdefault(sec, set()).update(re.findall(r"`([a-z_]+)`", first))
    return out
