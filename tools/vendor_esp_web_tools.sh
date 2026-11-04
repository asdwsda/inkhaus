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

# Patch (task 102): esptool-js 0.6 in EWT 10.4.0 only has the "classic" reset, which toggles DTR and RTS in
# separate calls. The M5Paper (CP2104) only resets when both lines change in ONE call, which is what esptool.py
# does on Linux (UnixTightReset) and why it connects. Add that as the first two connect strategies, keep classic
# after them, and make the final hard reset use one call too. Fails loudly if EWT's bundle changes.
bundle="$(ls "${dest}"/install-dialog-*.js)"
python3 -I - "${bundle}" <<'PY'
import sys
p = sys.argv[1]
s = open(p, encoding="utf-8").read()
def ut(delay):
    return ("{reset:async()=>{const t=this.transport,S=(d,r)=>(t._DTR_state=d,t.device.setSignals({dataTerminalReady:d,requestToSend:r})),"
            "W=m=>new Promise(r=>setTimeout(r,m));await S(!1,!1);await S(!0,!0);await S(!1,!0);await W(100);await S(!0,!1);await W(%s);await S(!1,!1)}}" % delay)
old1 = 'this.debug("using Classic Serial Reset"),[this.resetConstructors.classicReset(this.transport,e),this.resetConstructors.classicReset(this.transport,t)]'
new1 = 'this.debug("using UnixTight+Classic Serial Reset"),[%s,%s,this.resetConstructors.classicReset(this.transport,e),this.resetConstructors.classicReset(this.transport,t)]' % (ut("e"), ut("t"))
old2 = 'async connect(e="default_reset",t=7,i=!0)'
new2 = 'async connect(e="default_reset",t=8,i=!0)'
old3 = "async reset(){await this.transport.setRTS(!1),await this.transport.setDTR(!1),await Gr(100),await this.transport.setDTR(!0),await this.transport.setRTS(!1),await Gr(100),await this.transport.setRTS(!0),await this.transport.setDTR(!1),await this.transport.setRTS(!0),await Gr(100),await this.transport.setRTS(!1),await this.transport.setDTR(!1)}"
new3 = "async reset(){const t=this.transport,S=(d,r)=>(t._DTR_state=d,t.device.setSignals({dataTerminalReady:d,requestToSend:r}));await S(!1,!1);await Gr(100);await S(!1,!0);await Gr(100);await S(!1,!1)}"
for o, n in ((old1, new1), (old2, new2), (old3, new3)):
    if s.count(o) != 1:
        sys.exit("EWT bundle changed, patch pattern not found: " + o[:60])
    s = s.replace(o, n)
open(p, "w", encoding="utf-8").write(s)
PY
echo "vendored esp-web-tools ${VERSION} into ${dest}"
