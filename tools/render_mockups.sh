#!/usr/bin/env bash
# Render every docs/ui/*.svg mockup to docs/ui/png/<name>.png (1:1 device pixels)
# and docs/ui/png/<name>@2x.png, using Inkscape with DejaVu Sans (the M5GFX font).
# The PNGs are the reference images; SVG rendering differs between viewers.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
src="${root}/docs/ui"
out="${src}/png"
mkdir -p "${out}"

fonts="$(fc-list)"
if ! grep -qi "DejaVu Sans" <<<"${fonts}"; then
    echo "DejaVu Sans is not installed; renders would not match the device" >&2
    exit 1
fi

for svg in "${src}"/*.svg; do
    name="$(basename "${svg}" .svg)"
    inkscape "${svg}" --export-type=png --export-dpi=96 \
        --export-background=white --export-background-opacity=1 \
        --export-filename="${out}/${name}.png" >/dev/null 2>&1
    inkscape "${svg}" --export-type=png --export-dpi=192 \
        --export-background=white --export-background-opacity=1 \
        --export-filename="${out}/${name}@2x.png" >/dev/null 2>&1
    echo "${out}/${name}.png"
done
