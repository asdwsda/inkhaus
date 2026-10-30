#!/usr/bin/env bash
# Builds docs/ui/showcase-tour.gif (task 075): generator -> SVG frames -> Inkscape PNGs -> ImageMagick GIF.
# Needs the hero PNGs in docs/ui/png (see gen_showcase_composites.py header). No manual steps.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ui="${root}/docs/ui"
python3 "${root}/tools/showcase/gen_showcase_tour.py"
args=()
while IFS=$'\t' read -r name ms; do
    png="${ui}/png/${name%.svg}.png"
    inkscape "${ui}/${name}" --export-type=png --export-dpi=96 --export-background=white \
        --export-filename="${png}" >/dev/null 2>&1
    args+=(-delay "$(( ms / 10 ))" "${png}")
done <"${ui}/showcase-tour-frames.txt"
convert "${args[@]}" -resize 1000x -colors 24 -dither None -loop 0 -layers Optimize "${ui}/showcase-tour.gif"
ls -l "${ui}/showcase-tour.gif"
