#include <ArduinoJson.h>
#include <unity.h>
#include <yaml.h>

#include <cstring>
#include <string>

void setUp() {}
void tearDown() {}

void test_arduinojson_parses() {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, "{\"a\":{\"b\":42}}");
  TEST_ASSERT_TRUE(err == DeserializationError::Ok);
  TEST_ASSERT_EQUAL_INT(42, doc["a"]["b"].as<int>());
}

void test_libyaml_parses() {
  const char *text = "connection:\n  ha: demo\n";
  yaml_parser_t parser;
  TEST_ASSERT_EQUAL_INT(1, yaml_parser_initialize(&parser));
  yaml_parser_set_input_string(&parser, reinterpret_cast<const unsigned char *>(text),
                               std::strlen(text));

  std::string found;
  bool after_ha_key = false;
  yaml_event_t event;
  bool done = false;
  while (!done) {
    TEST_ASSERT_EQUAL_INT(1, yaml_parser_parse(&parser, &event));
    if (event.type == YAML_SCALAR_EVENT) {
      std::string v(reinterpret_cast<const char *>(event.data.scalar.value),
                    event.data.scalar.length);
      if (after_ha_key) {
        found = v;
        after_ha_key = false;
      } else if (v == "ha") {
        after_ha_key = true;
      }
    }
    done = event.type == YAML_STREAM_END_EVENT;
    yaml_event_delete(&event);
  }
  yaml_parser_delete(&parser);
  TEST_ASSERT_EQUAL_STRING("demo", found.c_str());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_arduinojson_parses);
  RUN_TEST(test_libyaml_parses);
  return UNITY_END();
}
