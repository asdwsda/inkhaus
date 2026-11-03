#!/usr/bin/env bash
# Task 102: fetch the pinned ESP Web Tools build, verify its sha512, and unpack dist/web into DEST.
# Usage: tools/vendor_esp_web_tools.sh DEST   (e.g. site/dist/install/esp-web-tools)
# To bump: change VERSION and INTEGRITY (npm view esp-web-tools@X dist.integrity), then test in Chrome.
set -euo pipefail

VERSION="10.4.0"
INTEGRITY="sha512-3pwkeFFm5Fj7UQo8SJNYK5RXrtNCpq6X9QoI6bMT4GBZWgrJqjn0YvM9ihG74BtMoSFYXfmDtkehuxe50PTMPQ=="

dest="${1:?usage: vendor_esp_web_tools.sh DEST}"
tmp="$(mktemp -d)"
trap 'rm -rf "${tmp}"' EXIT

curl -fsSL "https://registry.npmjs.org/esp-web-tools/-/esp-web-tools-${VERSION}.tgz" -o "${tmp}/ewt.tgz"
got="sha512-$(openssl dgst -sha512 -binary "${tmp}/ewt.tgz" | base64 -w0)"
if [[ "${got}" != "${INTEGRITY}" ]]; then
  echo "ESP Web Tools ${VERSION}: integrity mismatch (${got})" >&2
  exit 1
fi
tar -xzf "${tmp}/ewt.tgz" -C "${tmp}" package/dist/web
mkdir -p "${dest}"
cp -r "${tmp}/package/dist/web/." "${dest}/"
echo "vendored esp-web-tools ${VERSION} into ${dest}"
