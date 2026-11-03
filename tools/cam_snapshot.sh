#!/usr/bin/env bash
# Grab one frame of the M5Paper from the IP camera (settings in tools/cam.env, gitignored).
# Usage: tools/cam_snapshot.sh OUT.jpg [--raw]   (--raw: full camera frame, no crop/rotate)
# Burst: tools/cam_snapshot.sh OUT_%02d.jpg --burst N   (N frames at ~5 fps, e.g. to catch a refresh)
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "${root}/tools/cam.env"
out="${1:?usage: cam_snapshot.sh OUT.jpg [--raw|--burst N]}"
mode="${2:-}"
filter="crop=${CAM_CROP},transpose=1"
frames=(-frames:v 1)
case "${mode}" in
    --raw) filter="null" ;;
    --burst) frames=(-frames:v "${3:?burst count}" -r 5) ;;
    "") ;;
    *) echo "unknown option ${mode}" >&2; exit 2 ;;
esac
timeout 30 ffmpeg -hide_banner -loglevel fatal -rtsp_transport tcp -i "${CAM_URL}" \
    -vf "${filter}" "${frames[@]}" -q:v 3 -y "${out}"
echo "${out}"
