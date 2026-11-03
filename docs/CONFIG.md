# Configuration reference

The configuration reference is part of the user guide: [docs/guide/09-config-reference.md](guide/09-config-reference.md)
(on the website: <https://asdwsda.github.io/inkhaus/guide/config-reference/>).

Examples to copy: [`secrets.example.yaml`](../secrets.example.yaml) and [`config.example.yaml`](../config.example.yaml).
The parser in `lib/config/src/config.cpp` is the source of truth; `python3 site/check.py` lints the guide's key tables
against it.
