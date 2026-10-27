#pragma once
// libyaml event stream -> ArduinoJson document. All scalars become JSON strings
// (the validator converts where needed). Aliases are rejected.

#include <ArduinoJson.h>

#include <map>
#include <string>

#include "config_model.h"

namespace config {

// Maps a node path ("tabs[0].title") to the 1-based line where its value starts.
using LineMap = std::map<std::string, int>;

// Returns true on success. On failure appends one error (with line) to errors.
bool yamlToJson(const char *data, size_t len, JsonDocument &doc, LineMap &lines,
                std::vector<ConfigError> &errors);

}  // namespace config
