#!/usr/bin/env bash
# Regenerate certs/x509_crt_bundle (binary x509 root bundle for mbedTLS) from
# certs/cacert.pem (Mozilla roots, https://curl.se/ca/cacert.pem).
#
#   tools/gen_ca_bundle.sh            rebuild from the committed certs/cacert.pem (deterministic)
#   tools/gen_ca_bundle.sh --refresh  download the current cacert.pem first, then rebuild
#
# Refresh when Mozilla removes or adds a root the server chains use, and at least yearly.
# certs/gen_crt_bundle.py is ESP-IDF v4.4.7's generator (Apache-2.0), matching
# Arduino-ESP32 2.0.17's mbedTLS bundle format. Needs python3 with `cryptography`.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
certs="${root}/certs"

if [[ "${1:-}" == "--refresh" ]]; then
  curl -fsSL -o "${certs}/cacert.pem.new" https://curl.se/ca/cacert.pem
  mv "${certs}/cacert.pem.new" "${certs}/cacert.pem"
fi

cd "${certs}"
python3 gen_crt_bundle.py --input cacert.pem --quiet
