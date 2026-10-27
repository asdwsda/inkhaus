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

struct WifiConfig {
  std::string ssid;
  std::string password;
  bool hasStaticIp = false;
  StaticIp staticIp;
};

struct HomeAssistantConfig {
  std::string internalUrl;
  std::string externalUrl;  // empty if absent
  std::string token;
};

struct ConnectionConfig {
  WifiConfig wifi;
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
  // Entities this widget acts on: {entity} or the group members.
  std::vector<std::string> entities;
};

struct Tab {
  std::string title;
  std::vector<Widget> widgets;
};

struct Config {
  int version = 0;
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

}  // namespace config
