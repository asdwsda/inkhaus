#!/usr/bin/env bash
# Render the guide image sources docs/guide/img/src/*.svg to docs/guide/img/<name>.png
# (1:1 pixels; 2x versions in img/2x/ for the zoom view, 16 grey levels, metadata stripped) with Inkscape and DejaVu Sans (the device font),
# the same setup as render_mockups.sh. Rerunning reproduces the PNGs.
# Usage: tools/render_guide_images.sh [name ...]   (default: all)
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
src="${root}/docs/guide/img/src"
out="${root}/docs/guide/img"

fonts="$(fc-list)"
for font in "DejaVu Sans" "DejaVu Sans Mono"; do
    if ! grep -qi "${font}" <<<"${fonts}"; then
        echo "${font} is not installed; renders would not match the device" >&2
        exit 1
    fi
done

if [ "${#}" -gt 0 ]; then
    names=("${@}")
else
    names=()
    for svg in "${src}"/*.svg; do
        [ -e "${svg}" ] || continue
        names+=("$(basename "${svg}" .svg)")
    done
fi

tmp="$(mktemp -d)"
trap 'rm -rf "${tmp}"' EXIT

for name in "${names[@]}"; do
    inkscape "${src}/${name}.svg" --export-type=png --export-dpi=96 \
        --export-background=white --export-background-opacity=1 \
        --export-filename="${tmp}/${name}.png" >/dev/null 2>&1
    convert "${tmp}/${name}.png" -colorspace Gray -colors 16 -strip \
        -define png:compression-level=9 -define png:exclude-chunks=date,time \
        "PNG8:${out}/${name}.png"
    # 2x render, used only by the zoom view on the site (data-zoom); inline pictures stay 1x.
    mkdir -p "${out}/2x"
    inkscape "${src}/${name}.svg" --export-type=png --export-dpi=192 \
        --export-background=white --export-background-opacity=1 \
        --export-filename="${tmp}/${name}@2x.png" >/dev/null 2>&1
    convert "${tmp}/${name}@2x.png" -colorspace Gray -colors 16 -strip \
        -define png:compression-level=9 -define png:exclude-chunks=date,time \
        "PNG8:${out}/2x/${name}.png"
    size="$(stat -c %s "${out}/${name}.png")"
    echo "${out}/${name}.png (${size} bytes)"
done
