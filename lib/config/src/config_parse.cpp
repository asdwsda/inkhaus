#include "config_model.h"

#include <ArduinoJson.h>

#include <algorithm>

#include "config_internal.h"
#include "yaml_json.h"

namespace config {

std::vector<std::pair<std::string, std::vector<std::string>>> requestedAttributes(
    const Config &cfg) {
  std::vector<std::pair<std::string, std::vector<std::string>>> out;
  for (const Tab &t : cfg.tabs)
    for (const Widget &w : t.widgets) {
      std::vector<std::string> names;
      if (!w.attribute.empty()) names.push_back(w.attribute);
      for (const auto &d : w.details) names.push_back(d);
      if (names.empty()) continue;
      for (const auto &e : w.entities) {
        auto it = std::find_if(out.begin(), out.end(), [&](const auto &p) { return p.first == e; });
        if (it == out.end()) {
          out.push_back({e, {}});
          it = out.end() - 1;
        }
        for (const auto &n : names)
          if (std::find(it->second.begin(), it->second.end(), n) == it->second.end())
            it->second.push_back(n);
      }
    }
  return out;
}

ParseResult parseConfig(const char *data, size_t len) {
  ParseResult result;
  if (!data || len == 0) {
    result.errors.push_back({"", "config file is empty", 0});
    return result;
  }
  if (len > kMaxFileBytes) {
    result.errors.push_back(
        {"", "config file too large (max " + std::to_string(kMaxFileBytes) + " bytes)", 0});
    return result;
  }
  JsonDocument doc;
  LineMap lines;
  if (!yamlToJson(data, len, doc, lines, result.errors)) return result;
  validateConfig(lines, result, doc.as<JsonVariantConst>());
  return result;
}

ParseResult parseConfig(const std::string &text) {
  return parseConfig(text.data(), text.size());
}

SecretsResult parseSecrets(const char *data, size_t len) {
  SecretsResult out;
  if (!data || len == 0) {
    out.errors.push_back({"", "secrets file is empty", 0});
    return out;
  }
  if (len > kMaxFileBytes) {
    out.errors.push_back(
        {"", "secrets file too large (max " + std::to_string(kMaxFileBytes) + " bytes)", 0});
    return out;
  }
  JsonDocument doc;
  LineMap lines;
  if (!yamlToJson(data, len, doc, lines, out.errors)) return out;
  ParseResult tmp;
  validateSecrets(lines, tmp, doc.as<JsonVariantConst>());
  out.errors = std::move(tmp.errors);
  if (out.errors.empty()) out.connection = std::move(tmp.config.connection);
  return out;
}

SecretsResult parseSecrets(const std::string &text) {
  return parseSecrets(text.data(), text.size());
}

MergeOutcome mergeSecrets(ParseResult cfg, const SecretsResult *secrets) {
  MergeOutcome m;
  if (!secrets) {
    m.warnConnectionInConfig = cfg.ok() && cfg.config.hasConnection;
    m.parse = std::move(cfg);
    return m;
  }
  for (const ConfigError &e : secrets->errors)
    m.parse.errors.push_back({"secrets.yaml: " + e.path, e.message, e.line});
  if (!cfg.ok()) {
    for (auto &e : cfg.errors) m.parse.errors.push_back(std::move(e));
  } else if (cfg.config.hasConnection) {
    m.parse.errors.push_back(
        {"connection", "not allowed when secrets.yaml exists: move it to secrets.yaml", 0});
  }
  if (!m.parse.errors.empty()) return m;
  m.parse.config = std::move(cfg.config);
  m.parse.config.connection = secrets->connection;
  m.parse.config.hasConnection = true;
  return m;
}

}  // namespace config
