#!/usr/bin/env bash
# Usage: tools/cam_light.sh on|off
# Switches the room light for camera shots via HA. Reads CAM_LIGHT, CAM_HA_URL, CAM_HA_TOKEN
# from the gitignored tools/cam.env. Never prints the token.
set -euo pipefail
dir="$(cd "$(dirname "${0}")" && pwd)"
# shellcheck disable=SC1091
source "${dir}/cam.env"
action="${1:-}"
case "${action}" in on|off) ;; *) echo "usage: ${0} on|off" >&2; exit 2;; esac
if [[ -z "${CAM_LIGHT:-}" || -z "${CAM_HA_URL:-}" || -z "${CAM_HA_TOKEN:-}" ]]; then
  echo "cam_light: CAM_LIGHT/CAM_HA_URL/CAM_HA_TOKEN not set in tools/cam.env; skipping" >&2
  exit 0
fi
code="$(curl -s -o /dev/null -w '%{http_code}' -X POST \
  -H "Authorization: Bearer ${CAM_HA_TOKEN}" -H 'Content-Type: application/json' \
  -d "{\"entity_id\":\"${CAM_LIGHT}\"}" \
  "${CAM_HA_URL%/}/api/services/light/turn_${action}")"
echo "cam_light: ${action} http=${code}"
[[ "${code}" == 200 ]]
