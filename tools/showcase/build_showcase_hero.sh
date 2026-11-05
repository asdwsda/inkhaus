#!/usr/bin/env bash
# Builds docs/ui/png/showcase-hero.png (task 121): the photo-frame composite docs/ui/showcase-device.svg,
# rendered with Inkscape, scaled to 1000 px and reduced to 64 grey levels (landing page hero, site weight limit).
# Run gen_showcase_composites.py first.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ui="${root}/docs/ui"
tmp="$(mktemp -d)"
trap 'rm -rf "${tmp}"' EXIT
inkscape "${ui}/showcase-device.svg" --export-type=png --export-dpi=96 --export-background=white \
    --export-filename="${tmp}/hero.png" >/dev/null 2>&1
convert "${tmp}/hero.png" -resize 1000x -colorspace Gray -colors 64 -strip \
    -define png:compression-level=9 -define png:exclude-chunks=date,time "PNG8:${ui}/png/showcase-hero.png"
ls -l "${ui}/png/showcase-hero.png"
