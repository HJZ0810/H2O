#pragma once

#include "Actuators.h"
#include <cstddef>
#include <string>

namespace thingscloud {
struct Telemetry {
  float temperature = 0, humidity = 0;
  bool dhtValid = false;
  int mq2Mv = 0, mq135Mv = 0;
  bool mq2Valid = false, mq135Valid = false;
  int motorAPct = 0, motorBPct = 0;
  int servoPosition = 0, servoPulseUs = 0;
  bool servoEnabled = false, outputsReady = false;
};

constexpr size_t MaxRequestBytes = 1024;
constexpr char ReportTopic[] = "attributes";
constexpr char PushTopic[] = "attributes/push";
constexpr char ReportResultTopic[] = "attributes/response";
bool parsePush(const uint8_t* payload, size_t length, ActuatorCommand& command);
std::string report(const Telemetry& telemetry);
int parseReportResult(const uint8_t* payload, size_t length);
}
