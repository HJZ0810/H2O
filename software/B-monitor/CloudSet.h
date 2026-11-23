#pragma once
#include <ArduinoJson.h>
namespace cloudset {
struct Command { int fan = -1, servo = -1; };
inline bool field(JsonVariantConst v, int maximum, int& out) {
  if (v.is<JsonObjectConst>()) v = v["value"];
  if (!v.is<int>()) return false;
  int n = v.as<int>();
  if (n < 0 || n > maximum) return false;
  out = n;
  return true;
}
inline bool parse(JsonObjectConst params, Command& out) {
  Command candidate;
  for (JsonPairConst pair : params) {
    const char* key = pair.key().c_str();
    if (!strcmp(key, "fan_level")) { if (!field(pair.value(), 100, candidate.fan)) return false; }
    else if (!strcmp(key, "servo")) { if (!field(pair.value(), 180, candidate.servo)) return false; }
    else return false;
  }
  if (candidate.fan < 0 && candidate.servo < 0) return false;
  out = candidate;
  return true;
}
inline int normalizedPosition(int degrees) { return (degrees * 200 + 90) / 180 - 100; }
}
