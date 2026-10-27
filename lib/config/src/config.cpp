#include "config_model.h"

#include <ArduinoJson.h>

#include <algorithm>
#include <cstdlib>

#include "yaml_json.h"

namespace config {

namespace {
struct TypeName {
  const char *name;
  WidgetType type;
};
const TypeName kTypes[] = {
    {"cover", WidgetType::Cover},       {"light", WidgetType::Light},
    {"switch", WidgetType::Switch},     {"automation", WidgetType::Automation},
    {"climate", WidgetType::Climate},   {"sensor", WidgetType::Sensor},
    {"binary_sensor", WidgetType::BinarySensor},
    {"weather", WidgetType::Weather},   {"vacuum", WidgetType::Vacuum},
};

bool startsWith(const std::string &s, const char *prefix) {
  return s.rfind(prefix, 0) == 0;
}

bool validEntityId(const std::string &id) {
  size_t dot = id.find('.');
  if (dot == std::string::npos || dot == 0 || dot + 1 >= id.size()) return false;
  if (id.find('.', dot + 1) != std::string::npos) return false;
  for (char c : id) {
    if (c == '.') continue;
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
  }
  return true;
}

std::string domainOf(const std::string &id) { return id.substr(0, id.find('.')); }

bool validIpv4(const std::string &s) {
  int parts = 0;
  size_t i = 0;
  while (i <= s.size()) {
    size_t j = s.find('.', i);
    if (j == std::string::npos) j = s.size();
    if (j == i || j - i > 3) return false;
    int v = 0;
    for (size_t k = i; k < j; ++k) {
      if (s[k] < '0' || s[k] > '9') return false;
      v = v * 10 + (s[k] - '0');
    }
    if (v > 255) return false;
    ++parts;
    i = j + 1;
  }
  return parts == 4;
}

class Validator {
 public:
  Validator(const LineMap &lines, ParseResult &result) : lines_(lines), r_(result) {}

  void run(JsonVariantConst root) {
    if (!root.is<JsonObjectConst>()) {
      err("", "top level must be a mapping");
      return;
    }
    JsonObjectConst o = root.as<JsonObjectConst>();
    version(o);
    if (!o["connection"].isNull())
      connection(o["connection"], "connection");
    if (!o["groups"].isNull()) groups(o["groups"]);
    pages(o["pages"]);
  }

 private:
  const LineMap &lines_;
  ParseResult &r_;

  void err(const std::string &path, const std::string &msg) {
    int line = 0;
    std::string p = path;
    for (;;) {
      auto it = lines_.find(p);
      if (it != lines_.end()) {
        line = it->second;
        break;
      }
      if (p.empty()) break;
      size_t cut = p.find_last_of(".[");
      p = cut == std::string::npos ? "" : p.substr(0, cut);
    }
    r_.errors.push_back({path, msg, line});
  }

  static std::string join(const std::string &base, const char *key) {
    return base.empty() ? key : base + "." + key;
  }

  // Reads a string field. Returns true if present and a string.
  bool str(JsonObjectConst o, const char *key, const std::string &base, bool required,
           std::string &out) {
    std::string path = join(base, key);
    JsonVariantConst v = o[key];
    if (v.isNull()) {
      if (required) err(path, "missing required field");
      return false;
    }
    if (!v.is<const char *>()) {
      err(path, "must be a string");
      return false;
    }
    out = v.as<const char *>();
    if (required && out.empty()) {
      err(path, "must not be empty");
      return false;
    }
    return true;
  }

  void version(JsonObjectConst o) {
    std::string v;
    if (!str(o, "version", "", true, v)) return;
    char *end = nullptr;
    long n = std::strtol(v.c_str(), &end, 10);
    if (*end != '\0' || n != kSchemaVersion) {
      err("version", "unsupported version '" + v + "', expected " +
                         std::to_string(kSchemaVersion));
      return;
    }
    r_.config.version = static_cast<int>(n);
  }

  void connection(JsonVariantConst v, const std::string &path) {
    if (!v.is<JsonObjectConst>()) {
      err(path, "must be a mapping");
      return;
    }
    r_.config.hasConnection = true;
    JsonObjectConst c = v.as<JsonObjectConst>();
    ConnectionConfig &cc = r_.config.connection;

    if (c["wifi"].isNull()) {
      err("connection.wifi", "missing required field");
    } else if (!c["wifi"].is<JsonObjectConst>()) {
      err("connection.wifi", "must be a mapping");
    } else {
      JsonObjectConst w = c["wifi"];
      str(w, "ssid", "connection.wifi", true, cc.wifi.ssid);
      str(w, "password", "connection.wifi", false, cc.wifi.password);
      if (!w["static_ip"].isNull()) staticIp(w["static_ip"]);
    }

    if (c["home_assistant"].isNull()) {
      err("connection.home_assistant", "missing required field");
    } else if (!c["home_assistant"].is<JsonObjectConst>()) {
      err("connection.home_assistant", "must be a mapping");
    } else {
      JsonObjectConst h = c["home_assistant"];
      const std::string base = "connection.home_assistant";
      if (str(h, "internal_url", base, true, cc.ha.internalUrl) &&
          !startsWith(cc.ha.internalUrl, "http://") &&
          !startsWith(cc.ha.internalUrl, "https://"))
        err(base + ".internal_url", "must start with http:// or https://");
      if (str(h, "external_url", base, false, cc.ha.externalUrl) &&
          !startsWith(cc.ha.externalUrl, "https://"))
        err(base + ".external_url", "must start with https://");
      str(h, "token", base, true, cc.ha.token);
    }
  }

  void staticIp(JsonVariantConst v) {
    const std::string base = "connection.wifi.static_ip";
    if (!v.is<JsonObjectConst>()) {
      err(base, "must be a mapping with ip, gateway, subnet and dns");
      return;
    }
    JsonObjectConst o = v.as<JsonObjectConst>();
    StaticIp &s = r_.config.connection.wifi.staticIp;
    struct F {
      const char *key;
      std::string *out;
    } fields[] = {{"ip", &s.ip}, {"gateway", &s.gateway}, {"subnet", &s.subnet}, {"dns", &s.dns}};
    bool all = true;
    for (auto &f : fields) {
      if (!str(o, f.key, base, true, *f.out)) {
        all = false;
      } else if (!validIpv4(*f.out)) {
        err(join(base, f.key), "not a valid IPv4 address");
        all = false;
      }
    }
    r_.config.connection.wifi.hasStaticIp = all;
  }

  void groups(JsonVariantConst v) {
    if (!v.is<JsonObjectConst>()) {
      err("groups", "must be a mapping of group name to entity list");
      return;
    }
    for (JsonPairConst kv : v.as<JsonObjectConst>()) {
      std::string name = kv.key().c_str();
      std::string path = "groups." + name;
      Group g;
      g.name = name;
      if (!kv.value().is<JsonArrayConst>()) {
        err(path, "must be a list of entity ids");
        r_.config.groups.push_back(g);
        continue;
      }
      JsonArrayConst arr = kv.value().as<JsonArrayConst>();
      if (arr.size() == 0) err(path, "group must have at least one entity");
      if (arr.size() > kMaxEntitiesPerGroup)
        err(path, "too many entities (max " + std::to_string(kMaxEntitiesPerGroup) + ")");
      size_t i = 0;
      for (JsonVariantConst m : arr) {
        std::string mp = path + "[" + std::to_string(i++) + "]";
        if (!m.is<const char *>() || !validEntityId(m.as<const char *>())) {
          err(mp, "not a valid entity id (expected domain.object_id)");
          continue;
        }
        g.members.push_back(m.as<const char *>());
      }
      for (const auto &m : g.members)
        if (domainOf(m) != domainOf(g.members[0])) {
          err(path, "all group members must share one domain");
          break;
        }
      r_.config.groups.push_back(g);
    }
  }

  const Group *findGroup(const std::string &name) const {
    for (const auto &g : r_.config.groups)
      if (g.name == name) return &g;
    return nullptr;
  }

  void addEntity(const std::string &id) {
    auto &v = r_.config.entityIds;
    if (std::find(v.begin(), v.end(), id) == v.end()) v.push_back(id);
  }

  void pages(JsonVariantConst v) {
    if (v.isNull()) {
      err("pages", "missing required field");
      return;
    }
    if (!v.is<JsonArrayConst>()) {
      err("pages", "must be a list");
      return;
    }
    JsonArrayConst arr = v.as<JsonArrayConst>();
    if (arr.size() == 0) err("pages", "at least one page is required");
    if (arr.size() > kMaxPages)
      err("pages", "too many pages (max " + std::to_string(kMaxPages) + ")");
    size_t i = 0;
    for (JsonVariantConst p : arr) {
      std::string path = "pages[" + std::to_string(i++) + "]";
      if (i > kMaxPages) break;  // already reported
      page(p, path);
    }
  }

  void page(JsonVariantConst v, const std::string &path) {
    if (!v.is<JsonObjectConst>()) {
      err(path, "must be a mapping");
      return;
    }
    JsonObjectConst o = v.as<JsonObjectConst>();
    Page pg;
    str(o, "title", path, true, pg.title);
    JsonVariantConst w = o["widgets"];
    const std::string wpath = path + ".widgets";
    if (w.isNull()) {
      err(wpath, "missing required field");
    } else if (!w.is<JsonArrayConst>()) {
      err(wpath, "must be a list");
    } else {
      JsonArrayConst arr = w.as<JsonArrayConst>();
      if (arr.size() > kMaxWidgetsPerPage)
        err(wpath, "too many widgets (max " + std::to_string(kMaxWidgetsPerPage) + ")");
      size_t i = 0;
      for (JsonVariantConst wv : arr) {
        std::string p = wpath + "[" + std::to_string(i++) + "]";
        if (i > kMaxWidgetsPerPage) break;
        widget(wv, p, pg);
      }
    }
    r_.config.pages.push_back(std::move(pg));
  }

  void widget(JsonVariantConst v, const std::string &path, Page &pg) {
    if (!v.is<JsonObjectConst>()) {
      err(path, "must be a mapping");
      return;
    }
    JsonObjectConst o = v.as<JsonObjectConst>();
    Widget w;
    bool hasEntity = !o["entity"].isNull();
    bool hasGroup = !o["group"].isNull();
    if (hasEntity == hasGroup) {
      err(path, hasEntity ? "use either 'entity' or 'group', not both"
                          : "one of 'entity' or 'group' is required");
      return;
    }
    str(o, "name", path, false, w.name);

    std::string typeName;
    bool hasType = str(o, "type", path, false, typeName);
    WidgetType override = WidgetType::Sensor;
    if (hasType && !widgetTypeFromName(typeName, override)) {
      err(path + ".type", "unsupported type '" + typeName + "'");
      hasType = false;
    }

    std::string domain;
    if (hasEntity) {
      if (!str(o, "entity", path, true, w.entity)) return;
      if (!validEntityId(w.entity)) {
        err(path + ".entity", "not a valid entity id (expected domain.object_id)");
        return;
      }
      domain = domainOf(w.entity);
      w.entities.push_back(w.entity);
    } else {
      if (!str(o, "group", path, true, w.group)) return;
      const Group *g = findGroup(w.group);
      if (!g) {
        err(path + ".group", "unknown group '" + w.group + "'");
        return;
      }
      if (w.name.empty()) {
        err(path + ".name", "name is required for group widgets");
        return;
      }
      if (g->members.empty()) return;  // group already reported
      domain = domainOf(g->members[0]);
      w.entities = g->members;
    }

    if (hasType) {
      w.type = override;
    } else if (!widgetTypeFromName(domain, w.type)) {
      err(path + (hasEntity ? ".entity" : ".group"),
          "unsupported domain '" + domain + "', set 'type' explicitly");
      return;
    }
    for (const auto &e : w.entities) addEntity(e);
    pg.widgets.push_back(std::move(w));
  }
};

}  // namespace

const char *widgetTypeName(WidgetType type) {
  for (const auto &t : kTypes)
    if (t.type == type) return t.name;
  return "";
}

bool widgetTypeFromName(const std::string &name, WidgetType &out) {
  for (const auto &t : kTypes)
    if (name == t.name) {
      out = t.type;
      return true;
    }
  return false;
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
  Validator(lines, result).run(doc.as<JsonVariantConst>());
  return result;
}

ParseResult parseConfig(const std::string &text) {
  return parseConfig(text.data(), text.size());
}

}  // namespace config
