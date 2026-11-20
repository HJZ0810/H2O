#include "ThingsCloudProtocol.h"
#include <ArduinoJson.h>

namespace thingscloud {
namespace {
bool parseMessage(JsonDocument& doc, const uint8_t* data, size_t length) {
  return data && length && length <= MaxRequestBytes &&
         !deserializeJson(doc, data, length, DeserializationOption::NestingLimit(4)) &&
         doc.is<JsonObject>();
}

bool boundedInt(JsonVariantConst value, int minimum, int maximum, int& output) {
  if (!value.is<int>()) return false;
  output = value.as<int>();
  return output >= minimum && output <= maximum;
}

}

std::string report(const Telemetry& t) {
  JsonDocument doc;
  if (t.dhtValid) {
    doc["temperature"] = t.temperature;
    doc["humidity"] = t.humidity;
  }
  doc["dht_valid"] = t.dhtValid;
  if (t.mq2Valid) doc["mq2_mv"] = t.mq2Mv;
  doc["mq2_valid"] = t.mq2Valid;
  if (t.mq135Valid) doc["mq135_mv"] = t.mq135Mv;
  doc["mq135_valid"] = t.mq135Valid;
  doc["motor_a_pct"] = t.motorAPct;
  doc["motor_b_pct"] = t.motorBPct;
  doc["servo_position"] = t.servoPosition;
  doc["servo_enabled"] = t.servoEnabled;
  doc["servo_pulse_us"] = t.servoPulseUs;
  doc["outputs_ready"] = t.outputsReady;
  std::string output;
  serializeJson(doc, output);
  return output;
}

bool parsePush(const uint8_t* payload, size_t length, ActuatorCommand& output) {
  JsonDocument doc;
  if (!parseMessage(doc, payload, length) || !doc.size()) return false;
  ActuatorCommand command;
  for (JsonPairConst item : doc.as<JsonObjectConst>()) {
    if (item.key() == "motor_a_pct") {
      if (!boundedInt(item.value(), 0, 100, command.motorAPct)) return false;
      command.fields |= ActuatorCommand::MotorA;
    } else if (item.key() == "motor_b_pct") {
      if (!boundedInt(item.value(), 0, 100, command.motorBPct)) return false;
      command.fields |= ActuatorCommand::MotorB;
    } else if (item.key() == "servo_position") {
      if (!boundedInt(item.value(), -100, 100, command.servoPosition)) return false;
      command.fields |= ActuatorCommand::ServoPosition;
    } else if (item.key() == "servo_enabled") {
      if (!item.value().is<bool>()) return false;
      command.servoEnabled = item.value().as<bool>();
      command.fields |= ActuatorCommand::ServoEnabled;
    } else {
      return false;
    }
  }
  output = command;
  return true;
}

int parseReportResult(const uint8_t* payload, size_t length) {
  JsonDocument doc;
  if (!parseMessage(doc, payload, length) || !doc["result"].is<int>()) return -1;
  if (doc["result"] == 1) return 0;
  if (doc["result"] != 0 || !doc["errcode"].is<int>()) return -1;
  return doc["errcode"].as<int>();
}
}
