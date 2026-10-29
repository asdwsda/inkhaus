#pragma once
// Typed config model parsed from config.yaml (schema v1). Pure C++17, no Arduino.

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace config {

constexpr int kSchemaVersion = 1;
constexpr size_t kMaxTabs = 16;
constexpr size_t kMaxWidgetsPerTab = 24;
constexpr int kMaxWidgetSize = 4;
constexpr size_t kMaxEntitiesPerGroup = 16;
constexpr size_t kMaxFileBytes = 64 * 1024;
constexpr int kMaxYamlDepth = 12;
constexpr size_t kMaxAttrName = 40;
constexpr size_t kMaxDetails = 2;
constexpr size_t kMaxUnitChars = 8;
constexpr int kMaxDecimals = 3;

enum class WidgetType {
  Cover,
  Light,
  Switch,
  Automation,
  Climate,
  Sensor,
  BinarySensor,
  Weather,
  Vacuum,
};

// Name as used in the `type:` override and as HA domain (e.g. "binary_sensor").
const char *widgetTypeName(WidgetType type);
// Returns false if the name is not a supported type/domain.
bool widgetTypeFromName(const std::string &name, WidgetType &out);

struct StaticIp {
  std::string ip, gateway, subnet, dns;
};

// Which Home Assistant URL a Wi-Fi profile uses (task 047). Auto = learned per SSID.
enum class HaUrlMode { Auto, Internal, External };
constexpr size_t kMaxWifiProfiles = 4;
constexpr size_t kMaxProfileName = 20;

struct WifiConfig {
  std::string ssid;
  std::string password;
  bool hasStaticIp = false;
  StaticIp staticIp;
  std::string name;                  // display name; defaults to "Network N"
  HaUrlMode haUrl = HaUrlMode::Auto;
};

struct HomeAssistantConfig {
  std::string internalUrl;
  std::string externalUrl;  // empty if absent
  std::string token;
};

struct ConnectionConfig {
  WifiConfig wifi;                      // the first profile (kept for single-network callers)
  std::vector<WifiConfig> wifiProfiles; // all profiles in list order; [0] == wifi
  HomeAssistantConfig ha;
};

struct Group {
  std::string name;
  std::vector<std::string> members;
};

// Widget footprint in grid cells, each 1..kMaxWidgetSize.
struct WidgetSize {
  int w = 1;
  int h = 1;
};

struct Widget {
  WidgetType type = WidgetType::Sensor;
  std::string entity;  // single-entity widget, empty for groups
  std::string group;   // group widget, empty for single entities
  std::string name;    // empty: use HA friendly_name
  std::optional<WidgetSize> size;  // absent: type default (layout engine decides)
  // Optional attribute display (task 030). Names are [a-z0-9_]+, <= kMaxAttrName chars.
  std::string attribute;                // show this attribute as the main value instead of the state
  std::vector<std::string> details;     // up to kMaxDetails extra attributes for a details line
  std::optional<std::string> unit;      // unit override for the main value (<= kMaxUnitChars chars)
  std::optional<int> decimals;          // 0..3, rounds numeric main/detail values
  // Entities this widget acts on: {entity} or the group members.
  std::vector<std::string> entities;
};

constexpr int kDefaultLandscapeColumns = 5;

struct Tab {
  std::string title;
  std::vector<Widget> widgets;
  int landscapeColumns = kDefaultLandscapeColumns;  // 4 or 5 (landscape grid only)
};

struct Config {
  int version = 0;
  // POSIX TZ string (e.g. "CET-1CEST,M3.5.0,M10.5.0/3"); optional `timezone:` key.
  std::string timezone = "CET-1CEST,M3.5.0,M10.5.0/3";
  bool hasConnection = false;
  ConnectionConfig connection;
  std::vector<Group> groups;
  std::vector<Tab> tabs;
  // De-duplicated, in first-use order; includes members of referenced groups.
  std::vector<std::string> entityIds;
};

struct ConfigError {
  std::string path;     // e.g. "tabs[0].widgets[2].entity"; empty = whole file
  std::string message;
  int line = 0;         // 1-based; 0 if unknown

  ConfigError() = default;
  ConfigError(std::string p, std::string m, int l = 0)
      : path(std::move(p)), message(std::move(m)), line(l) {}
};

struct ParseResult {
  Config config;
  std::vector<ConfigError> errors;
  bool ok() const { return errors.empty(); }
};

// Parses and validates. Never throws or crashes on bad input; config is only
// meaningful when errors is empty.
ParseResult parseConfig(const char *data, size_t len);
ParseResult parseConfig(const std::string &text);

// secrets.yaml: `wifi:` + `home_assistant:` at the top level (the old `connection:` structure).
// Error paths are relative to that file (e.g. "wifi.ssid"); messages never contain values.
struct SecretsResult {
  ConnectionConfig connection;
  std::vector<ConfigError> errors;
  bool ok() const { return errors.empty(); }
};
SecretsResult parseSecrets(const char *data, size_t len);
SecretsResult parseSecrets(const std::string &text);

struct MergeOutcome {
  ParseResult parse;
  bool warnConnectionInConfig = false;  // no secrets.yaml, connection still in config.yaml
};
// `secrets` is null when /secrets.yaml does not exist. With secrets: its connection wins and a
// `connection:` in config.yaml is an error; secrets errors get the path prefix "secrets.yaml: ".
// Without: config.yaml's own connection is kept (warning flag set if it has one).
MergeOutcome mergeSecrets(ParseResult cfg, const SecretsResult *secrets);

// Per entity id: the attribute names its widgets request (attribute + details), de-duplicated.
// Only entities with at least one request appear.
std::vector<std::pair<std::string, std::vector<std::string>>> requestedAttributes(
    const Config &cfg);

}  // namespace config
