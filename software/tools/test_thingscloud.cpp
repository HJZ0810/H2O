#include "ThingsCloudProtocol.h"
#include "Board.h"
#include "Arduino.h"
#include <ArduinoJson.h>
#include <array>
#include <cassert>
#include <iostream>
#include <string>

namespace {
struct Pin { int level=0; uint32_t duty=0, frequency=0; };
std::array<Pin, 49> pins;
int failPin=-1;
}

void digitalWrite(int pin, int value) { pins.at(pin).level=value; }
void pinMode(int, int) {}
bool ledcAttachChannel(int pin, uint32_t frequency, uint8_t, uint8_t) {
  pins.at(pin).frequency=frequency;
  return true;
}
bool ledcWrite(int pin, uint32_t duty) {
  if (pin==failPin) return false;
  pins.at(pin).duty=duty;
  return true;
}
bool ledcDetach(int pin) {
  pins.at(pin).duty=0;
  return true;
}

namespace {
bool decode(const std::string& json, ActuatorCommand& command) {
  return thingscloud::parsePush(reinterpret_cast<const uint8_t*>(json.data()), json.size(), command);
}

thingscloud::Telemetry snapshot(const Actuators& driver) {
  thingscloud::Telemetry data;
  data.temperature=24.6f; data.humidity=51.5f; data.dhtValid=true;
  data.mq2Mv=1438; data.mq135Mv=1820; data.mq2Valid=data.mq135Valid=true;
  data.motorAPct=driver.speedPercent(false);
  data.motorBPct=driver.speedPercent(true);
  data.servoPosition=driver.servoPosition();
  data.servoPulseUs=driver.pulseWidth();
  data.servoEnabled=driver.pulseWidth()!=0;
  data.outputsReady=driver.ready();
  return data;
}

int simulate(Actuators& fan) {
  std::cout << thingscloud::report(snapshot(fan)) << std::endl;
  std::string json;
  while (std::getline(std::cin,json)) {
    ActuatorCommand control;
    if (decode(json,control)) fan.apply(control);
    std::cout << thingscloud::report(snapshot(fan)) << std::endl;
  }
  return 0;
}
}

int main(int argc,char** argv) {
  Actuators fan;
  fan.begin();
  if (argc==2 && std::string(argv[1])=="--simulate") return simulate(fan);

  assert(fan.ready());
  assert(pins[board::MotorPwm].frequency==20000);
  assert(pins[board::MotorPwmB].frequency==20000);
  assert(pins[board::ServoPwm].frequency==50);
  assert(pins[board::MotorAin1].level==LOW && pins[board::MotorBin1].level==LOW);

  ActuatorCommand command;
  assert(decode(R"JSON({"motor_a_pct":60,"motor_b_pct":42,"servo_position":-50})JSON",command));
  assert(fan.apply(command));
  assert(pins[board::MotorPwm].duty==153 && pins[board::MotorPwmB].duty==107);
  assert(pins[board::MotorAin1].level==HIGH && pins[board::MotorAin2].level==LOW);
  assert(pins[board::MotorBin1].level==HIGH && pins[board::MotorBin2].level==LOW);
  assert(fan.pulseWidth()==1350 && fan.servoPosition()==-50);
  assert(pins[board::ServoPwm].duty==((1350u<<14)/20000));

  for (const std::string invalid : {
       R"JSON({"motor_a_pct":21,"motor_b_pct":101})JSON", R"JSON({"temperature":24})JSON",
       R"JSON({"motor_b_pct":-1})JSON", R"JSON({"servo_position":101})JSON",
       R"JSON({"motor_a_pct":"4"})JSON", R"JSON({"servo_enabled":1})JSON", "[]", "{broken", "{}"}) {
    assert(!decode(invalid,command));
  }
  assert(!decode(std::string(1025,' '),command));
  assert(fan.speedPercent(false)==60 && fan.speedPercent(true)==42);

  assert(decode(R"JSON({"servo_position":100,"servo_enabled":false})JSON",command));
  assert(fan.apply(command) && fan.pulseWidth()==0 && fan.servoPosition()==100);
  assert(decode(R"JSON({"servo_enabled":true})JSON",command));
  assert(fan.apply(command) && fan.pulseWidth()==1800);
  fan.turn(-1);
  assert(fan.pulseWidth()<1800);
  fan.selectNextChannel();
  fan.changeSpeed(-1);
  assert(fan.speedPercent(true)==40 && fan.speedPercent(false)==60);

  JsonDocument report;
  assert(!deserializeJson(report,thingscloud::report(snapshot(fan))));
  assert(report["dht_valid"].as<bool>() && report["mq2_mv"]==1438 && report["mq135_mv"]==1820);
  assert(report["motor_b_pct"]==40 && report["servo_enabled"].as<bool>());
  assert(!report["supply_voltage"].is<float>());
  assert(!deserializeJson(report,thingscloud::report({})));
  assert(report["temperature"].isNull() && report["mq2_mv"].isNull() && !report["mq2_valid"].as<bool>());

  auto checkResponse=[](const std::string& text) {
    return thingscloud::parseReportResult(reinterpret_cast<const uint8_t*>(text.data()),text.size());
  };
  assert(checkResponse(R"JSON({"result":1,"ts":123})JSON")==0);
  assert(checkResponse(R"JSON({"result":0,"errcode":413})JSON")==413);
  assert(checkResponse("{}")==-1);

  assert(decode(R"JSON({"servo_position":0})JSON",command));
  failPin=board::ServoPwm;
  assert(!fan.apply(command) && !fan.ready());
  assert(pins[board::MotorAin1].level==LOW && pins[board::MotorBin1].level==LOW);
  assert(fan.pulseWidth()==0 && pins[board::ServoPwm].duty==0);
  std::cout << "ThingsCloud telemetry, strict control, dual PWM and stop-on-fault: PASS\n";
}
