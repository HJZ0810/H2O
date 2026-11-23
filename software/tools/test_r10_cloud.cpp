#include <cassert>
#include <cstring>
#include "CloudSet.h"
#include "LinkageCore.h"

static bool parse(const char* json, cloudset::Command& out) {
  JsonDocument doc;
  assert(!deserializeJson(doc, json));
  return cloudset::parse(doc.as<JsonObjectConst>(), out);
}
int main() {
  cloudset::Command cmd;
  assert(parse(R"({"fan_level":0,"servo":{"value":180}})", cmd));
  assert(cmd.fan == 0 && cmd.servo == 180);
  assert(parse(R"({"fan_level":{"value":100},"servo":0})", cmd));
  assert(cmd.fan == 100 && cmd.servo == 0);
  for (const auto* bad : {"null", "[]", "{}", R"({"servo":181})", R"({"servo":true})", R"({"fan_level":-1})", R"({"fan_level":12.5})", R"({"fan_level":40,"servo":"9"})", R"({"unknown":0})"}) {
    assert(!parse(bad, cmd));
    assert(cmd.fan == 100 && cmd.servo == 0);
  }
  assert(cloudset::normalizedPosition(0) == -100);
  assert(cloudset::normalizedPosition(90) == 0);
  assert(cloudset::normalizedPosition(180) == 100);
  assert(linkage::decide(false, 0, -1, -1, 45).servoDeg == 45);
  assert(linkage::decide(true, 35, 305, 505, 0).fanPct == 80);
  return 0;
}
