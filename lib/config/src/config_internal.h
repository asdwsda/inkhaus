#pragma once
// Private to lib/config: the validator entry points used by config_parse.cpp.
#include <ArduinoJson.h>

#include "config_model.h"
#include "yaml_json.h"

namespace config {

void validateConfig(const LineMap &lines, ParseResult &result, JsonVariantConst root);
void validateSecrets(const LineMap &lines, ParseResult &result, JsonVariantConst root);

}  // namespace config
