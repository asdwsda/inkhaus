#!/usr/bin/env bash
# Run a command while holding the M5Paper device lock, so only one agent at a time
# flashes, resets or talks to the board.
#
# Usage:
#   tools/device_lock.sh pio run -e m5paper -t upload --upload-port /dev/ttyUSB0
#   tools/device_lock.sh tools/serial_capture.py --reset --seconds 15
#   tools/device_lock.sh --wait 600 tools/screenshot.py
#
# Waits up to --wait seconds (default 300) for the lock, then gives up with exit code 75.
# The lock is released automatically when the command ends (also if it crashes).
# The holder's description goes to the lock file, so a waiting agent can see who has it.
set -euo pipefail

wait_s=300
if [[ "${1:-}" == "--wait" ]]; then
    wait_s="${2:?--wait needs seconds}"
    shift 2
fi
if [[ $# -eq 0 ]]; then
    echo "usage: ${0} [--wait SECONDS] command [args...]" >&2
    exit 2
fi

lock_dir="${XDG_RUNTIME_DIR:-/tmp}"
lock_file="${lock_dir}/m5paper-device.lock"

exec 9>>"${lock_file}"
if ! flock -w "${wait_s}" 9; then
    echo "device lock busy for ${wait_s}s; held by: $(cat "${lock_file}" 2>/dev/null)" >&2
    exit 75
fi
printf '%s pid=%s cwd=%s cmd=%s\n' "$(date -Is)" "$$" "${PWD}" "$*" >"${lock_file}"

set +e
"$@"
rc=$?
set -e
: >"${lock_file}"
exit "${rc}"
