#!/usr/bin/env bash
# Render every docs/ui/*.svg mockup to docs/ui/png/<name>.png and every docs/ui/experiments/*.svg
# to docs/ui/experiments/png/<name>.png (1:1 device pixels),
# using Inkscape with DejaVu Sans (the M5GFX font).
# The PNGs are the reference images; SVG rendering differs between viewers.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
src="${root}/docs/ui"

fonts="$(fc-list)"
if ! grep -qi "DejaVu Sans" <<<"${fonts}"; then
    echo "DejaVu Sans is not installed; renders would not match the device" >&2
    exit 1
fi

for dir in "${src}" "${src}/experiments"; do
    out="${dir}/png"
    mkdir -p "${out}"
    for svg in "${dir}"/*.svg; do
        [ -e "${svg}" ] || continue
        name="$(basename "${svg}" .svg)"
        inkscape "${svg}" --export-type=png --export-dpi=96 \
            --export-background=white --export-background-opacity=1 \
            --export-filename="${out}/${name}.png" >/dev/null 2>&1
        echo "${out}/${name}.png"
    done
done
