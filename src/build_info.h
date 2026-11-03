#pragma once
// Task 113: build identity (variant from the env's -DFW_VARIANT flag, commit/date from tools/build_info.py).
#include "build_info_line.h"  // lib/settings_ui

namespace buildinfo {
settings_ui::BuildInfo info();
// "1.3.1 debug · 7727f0d · 2026-11-03" for the boot log.
const char *line();
}  // namespace buildinfo
