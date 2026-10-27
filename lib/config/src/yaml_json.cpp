#include "yaml_json.h"

#include <yaml.h>

#include <cstring>

namespace config {
namespace {

struct Event {
  yaml_event_t ev;
  bool live = false;
  Event() { std::memset(&ev, 0, sizeof(ev)); }
  ~Event() { reset(); }
  void reset() {
    if (live) yaml_event_delete(&ev);
    live = false;
  }
  Event(const Event &) = delete;
  Event &operator=(const Event &) = delete;
};

class Builder {
 public:
  Builder(yaml_parser_t &parser, LineMap &lines, std::vector<ConfigError> &errors)
      : parser_(parser), lines_(lines), errors_(errors) {}

  bool next(Event &e) {
    e.reset();
    if (!yaml_parser_parse(&parser_, &e.ev)) {
      int line = static_cast<int>(parser_.problem_mark.line) + 1;
      std::string msg = "YAML syntax error";
      if (parser_.problem) msg += std::string(": ") + parser_.problem;
      if (parser_.context) msg += std::string(" (") + parser_.context + ")";
      errors_.push_back({"", msg, line});
      return false;
    }
    e.live = true;
    return true;
  }

  bool fail(const std::string &path, const std::string &msg, const yaml_event_t &ev) {
    errors_.push_back({path, msg, static_cast<int>(ev.start_mark.line) + 1});
    return false;
  }

  // `ev` is the already-fetched first event of the value.
  bool value(JsonVariant dst, const Event &first, const std::string &path, int depth) {
    const yaml_event_t &ev = first.ev;
    lines_[path] = static_cast<int>(ev.start_mark.line) + 1;
    if (depth > kMaxYamlDepth) return fail(path, "nesting too deep", ev);
    switch (ev.type) {
      case YAML_SCALAR_EVENT:
        dst.set(std::string(reinterpret_cast<const char *>(ev.data.scalar.value),
                            ev.data.scalar.length));
        return true;
      case YAML_ALIAS_EVENT:
        return fail(path, "YAML aliases are not supported", ev);
      case YAML_SEQUENCE_START_EVENT: {
        JsonArray arr = dst.to<JsonArray>();
        Event item;
        for (size_t i = 0;; ++i) {
          if (!next(item)) return false;
          if (item.ev.type == YAML_SEQUENCE_END_EVENT) return true;
          JsonVariant child = arr.add<JsonVariant>();
          if (!value(child, item, path + "[" + std::to_string(i) + "]", depth + 1)) return false;
        }
      }
      case YAML_MAPPING_START_EVENT: {
        JsonObject obj = dst.to<JsonObject>();
        Event key, val;
        for (;;) {
          if (!next(key)) return false;
          if (key.ev.type == YAML_MAPPING_END_EVENT) return true;
          if (key.ev.type != YAML_SCALAR_EVENT)
            return fail(path, "only plain scalar keys are supported", key.ev);
          std::string k(reinterpret_cast<const char *>(key.ev.data.scalar.value),
                        key.ev.data.scalar.length);
          std::string childPath = path.empty() ? k : path + "." + k;
          if (lines_.count(childPath)) return fail(childPath, "duplicate key", key.ev);
          if (!next(val)) return false;
          JsonVariant child = obj[k].to<JsonVariant>();
          if (!value(child, val, childPath, depth + 1)) return false;
        }
      }
      default:
        return fail(path, "unexpected YAML structure", ev);
    }
  }

 private:
  yaml_parser_t &parser_;
  LineMap &lines_;
  std::vector<ConfigError> &errors_;
};

}  // namespace

bool yamlToJson(const char *data, size_t len, JsonDocument &doc, LineMap &lines,
                std::vector<ConfigError> &errors) {
  yaml_parser_t parser;
  if (!yaml_parser_initialize(&parser)) {
    errors.push_back({"", "out of memory initializing YAML parser", 0});
    return false;
  }
  yaml_parser_set_input_string(&parser, reinterpret_cast<const unsigned char *>(data), len);

  Builder b(parser, lines, errors);
  bool ok = false;
  {
    Event e;
    if (b.next(e) && e.ev.type == YAML_STREAM_START_EVENT && b.next(e)) {
      if (e.ev.type == YAML_STREAM_END_EVENT) {
        errors.push_back({"", "config file is empty", 1});
      } else if (e.ev.type == YAML_DOCUMENT_START_EVENT && b.next(e)) {
        ok = b.value(doc.to<JsonVariant>(), e, "", 0);
      }
    }
  }
  yaml_parser_delete(&parser);
  if (!ok && errors.empty()) errors.push_back({"", "unexpected YAML structure", 1});
  return ok;
}

}  // namespace config
