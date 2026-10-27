#!/usr/bin/env bash
# Build docs/ui/flow.gif from the storyboard in docs/ui/flow/.
# Frame order, file names and durations come from the table in docs/ui/flow/flow.md:
# the column headed "File" (or "Frame"; else the first column) is the SVG file name, and the column headed
# "ms" is the duration. Frames are rendered with Inkscape (DejaVu Sans) and assembled
# with ImageMagick.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
flow="${root}/docs/ui/flow"
manifest="${flow}/flow.md"
out_gif="${root}/docs/ui/flow.gif"
work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT

fonts="$(fc-list)"
if ! grep -qi "DejaVu Sans" <<<"${fonts}"; then
    echo "DejaVu Sans is not installed; frames would not match the device" >&2
    exit 1
fi

# Parse the markdown table into "file<TAB>ms" lines.
python3 - "${manifest}" >"${work}/frames.tsv" <<'PY'
import re, sys
rows = [l.strip() for l in open(sys.argv[1], encoding="utf-8") if l.strip().startswith("|")]
header = [c.strip().lower() for c in rows[0].strip("|").split("|")]
ms_col = next(i for i, h in enumerate(header) if re.search(r"\bms\b", h))
file_col = next((i for i, h in enumerate(header) if h in ("file", "frame")), 0)
for row in rows[2:]:
    cells = [c.strip() for c in row.strip("|").split("|")]
    name = re.sub(r"[`\[\]]|\(.*?\)", "", cells[file_col]).strip()
    ms = int(re.sub(r"[^0-9]", "", cells[ms_col]))
    print(f"{name}\t{ms}")
PY

args=()
n=0
while IFS=$'\t' read -r name ms; do
    svg="${flow}/${name}"
    [[ -f "${svg}" ]] || { echo "missing frame: ${svg}" >&2; exit 1; }
    png="${work}/$(printf '%03d' "${n}").png"
    inkscape "${svg}" --export-type=png --export-dpi=96 \
        --export-background=white --export-background-opacity=1 \
        --export-filename="${png}" >/dev/null 2>&1
    # GIF delays are in 1/100 s; never below 2 (browsers clamp smaller values).
    delay=$(( (ms + 5) / 10 ))
    (( delay < 2 )) && delay=2
    args+=(-delay "${delay}" "${png}")
    n=$((n + 1))
done <"${work}/frames.tsv"

convert -loop 0 "${args[@]}" -layers Optimize "${out_gif}"
echo "${out_gif}: ${n} frames"
