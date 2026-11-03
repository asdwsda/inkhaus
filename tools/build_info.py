"""PlatformIO extra script (task 113): writes include/build_info_gen.h (short git commit, dirty flag, build
date). Only src/build_info.cpp includes it, so a new commit recompiles one file. Gitignored."""
import datetime
import os
import subprocess

Import("env")  # noqa: F821


def _git(*args):
    try:
        return subprocess.check_output(["git", *args], cwd=env["PROJECT_DIR"],  # noqa: F821
                                       stderr=subprocess.DEVNULL, text=True).strip()
    except Exception:
        return ""


commit = _git("rev-parse", "--short=7", "HEAD")
dirty = 1 if commit and _git("status", "--porcelain", "--untracked-files=no") else 0
text = ('#pragma once\n#define FW_COMMIT "%s"\n#define FW_COMMIT_DIRTY %d\n#define FW_DATE "%s"\n'
        % (commit, dirty, datetime.date.today().isoformat()))
path = os.path.join(env["PROJECT_DIR"], "include", "build_info_gen.h")  # noqa: F821
try:
    old = open(path).read()
except OSError:
    old = ""
if old != text:  # keep the mtime when nothing changed
    with open(path, "w") as f:
        f.write(text)
