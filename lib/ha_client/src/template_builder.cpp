#include "template_builder.h"

#include <ArduinoJson.h>

namespace {

const char* domain_attrs(const std::string& domain) {
  if (domain == "cover") return "current_position";
  if (domain == "light") return "brightness";
  if (domain == "climate")
    return "current_temperature,temperature,current_humidity,hvac_mode,hvac_action,"
           "min_temp,max_temp,target_temp_step";
  if (domain == "sensor") return "unit_of_measurement,device_class";
  if (domain == "weather") return "temperature,humidity,temperature_unit";
  if (domain == "vacuum") return "battery_level,status";
  return "";
}

}  // namespace

bool is_valid_entity_id(const std::string& id) {
  size_t dot = id.find('.');
  if (dot == std::string::npos || dot == 0 || dot + 1 >= id.size()) return false;
  if (id.find('.', dot + 1) != std::string::npos) return false;
  for (char c : id) {
    if (c == '.') continue;
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
  }
  return true;
}

std::string entity_domain(const std::string& id) {
  size_t dot = id.find('.');
  return dot == std::string::npos ? std::string() : id.substr(0, dot);
}

bool build_state_template(const std::vector<std::string>& ids, std::string& out,
                          std::string& error) {
  if (ids.empty()) {
    error = "no entity ids";
    return false;
  }
  out = "{";
  bool first = true;
  for (const std::string& id : ids) {
    if (!is_valid_entity_id(id)) {
      error = "invalid entity id: " + id;
      return false;
    }
    if (!first) out += ", ";
    first = false;
    // states.<domain>['<object>'] (bracket form: object ids may start with a digit) is
    // None for unknown entities -> {"s": null}. The id is validated to [a-z0-9_.] above,
    // so it cannot break out of the quotes.
    const size_t dot = id.find('.');
    out += "\"" + id + "\": {% set s = states." + id.substr(0, dot) + "['" + id.substr(dot + 1) +
           "'] %}{% if s %}{{ {'s': s.state, ";
    out += "'lc': as_timestamp(s.last_changed)|int, 'a': {'friendly_name': "
           "s.attributes.get('friendly_name')";
    std::string attrs = domain_attrs(entity_domain(id));
    size_t pos = 0;
    while (!attrs.empty() && pos <= attrs.size()) {
      size_t comma = attrs.find(',', pos);
      std::string name = attrs.substr(pos, comma == std::string::npos ? comma : comma - pos);
      out += ", '" + name + "': s.attributes.get('" + name + "')";
      if (comma == std::string::npos) break;
      pos = comma + 1;
    }
    out += "}}|tojson }}{% else %}{\"s\": null}{% endif %}";
  }
  out += "}";
  return true;
}

bool build_template_request_body(const std::vector<std::string>& ids, std::string& body,
                                 std::string& error) {
  std::string tpl;
  if (!build_state_template(ids, tpl, error)) return false;
  JsonDocument doc;
  doc["template"] = tpl;
  body.clear();
  serializeJson(doc, body);
  return true;
}
