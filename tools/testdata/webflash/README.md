# Web flasher dry run (task 102)

`api.json` is a fake `gh api repos/OWNER/REPO/releases` list; `src/<tag>/` are the downloaded release
folders (tiny fake segment files). `tools/testdata/gen_webflash_sample.py` regenerates them.

Local dry run, same steps as `.github/workflows/pages.yml`:

    python3 site/build.py
    tools/vendor_esp_web_tools.sh site/dist/install/esp-web-tools        # needs network; optional
    python3 tools/make_webflash_manifest.py select --api tools/testdata/webflash/api.json
    python3 tools/make_webflash_manifest.py build --api tools/testdata/webflash/api.json \
        --src tools/testdata/webflash/src --out site/dist/install
    python3 site/check.py
    python3 -m http.server -d site/dist 8000        # then http://localhost:8000/install/ in Chrome

With real releases: `gh api "repos/OWNER/REPO/releases?per_page=100" > api.json`, then
`gh release download TAG -D src/TAG -p manifest.json -p '*.bin'` for each tag from `select`.
Unit tests: `python3 tools/tests/test_make_webflash_manifest.py`.
