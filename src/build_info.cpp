#include "build_info.h"

#include "version.h"

// FW_COMMIT / FW_COMMIT_DIRTY / FW_DATE: written by tools/build_info.py into include/build_info_gen.h.
#if __has_include("build_info_gen.h")
#include "build_info_gen.h"
#else
#define FW_COMMIT ""
#define FW_COMMIT_DIRTY 0
#define FW_DATE ""
#endif
#ifndef FW_VARIANT
#define FW_VARIANT "release"
#endif

namespace buildinfo {

settings_ui::BuildInfo info() {
  settings_ui::BuildInfo b;
  b.version = FW_VERSION;
  b.variant = FW_VARIANT;
  b.commit = FW_COMMIT;
  b.dirty = FW_COMMIT_DIRTY != 0;
  b.date = FW_DATE;
  return b;
}

const char *line() {
  static std::string s = settings_ui::buildInfoLine(info());
  return s.c_str();
}

}  // namespace buildinfo
