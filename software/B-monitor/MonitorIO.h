#pragma once
#include "BCfg.h"
#include "LinkageCore.h"

namespace bmon {

// Arduino core 3.x supplies the ADC/LEDC and GPIO functions used here.
inline int readMq(int pin) {
  if (pin < 0) return -1;
  uint32_t sumMv = 0;
  for (int i = 0; i < 16; ++i) {
    const int raw = analogRead(pin);
    const uint32_t mv = analogReadMilliVolts(pin);
    if (raw >= 4090 || mv >= 3050) return -1;
    sumMv += mv;
  }
  return int((sumMv / 16 * 1000 + 1650) / 3300);
}

inline bool validDht(float t, float h) {
  return std::isfinite(t) && std::isfinite(h) && t >= 0 && t <= 50 && h >= 0 && h <= 100;
}

struct OutputChannel {
  bool attached = false;
  int confirmed = -1;  // unknown until the PWM driver accepts the write
  bool manual = false;
  uint32_t manualAt = 0;

  bool held(uint32_t now) {
    if (manual && uint32_t(now - manualAt) >= ManualHoldMs) manual = false;
    return manual;
  }
  void hold(uint32_t now) { manual = true; manualAt = now; }
};

struct MonitorOutputs {
  OutputChannel fan, servo;
  int tempDemand = 0, smokeDemand = 0, windowDemand = ServoClose;

  bool writeFan(int level) {
    if (!fan.attached || !ledcWrite(FanPwm, uint32_t(level) * 255 / 100)) {
      fan.confirmed = -1;
      if (FanIn1 >= 0) digitalWrite(FanIn1, LOW);
      if (FanIn2 >= 0) digitalWrite(FanIn2, LOW);
      return false;
    }
    if (FanIn1 >= 0) digitalWrite(FanIn1, level > 0 ? HIGH : LOW);
    if (FanIn2 >= 0) digitalWrite(FanIn2, LOW);
    fan.confirmed = level;
    return true;
  }
  bool writeServo(int angle) {
    const uint32_t us = 500 + uint32_t(angle) * 2000 / 180;
    if (!servo.attached || !ledcWrite(ServoPwm, us * 16383u / 20000u)) {
      servo.confirmed = -1;
      return false;
    }
    servo.confirmed = angle;
    return true;
  }
  void begin() {
    if (FanIn1 >= 0) pinMode(FanIn1, OUTPUT);
    if (FanIn2 >= 0) pinMode(FanIn2, OUTPUT);
    fan.attached = ledcAttach(FanPwm, 20000, 8);
    servo.attached = ledcAttach(ServoPwm, 50, 14);
    writeFan(0);
    writeServo(ServoClose);
  }
  bool command(int level, int angle, uint32_t now) {
    if ((level >= 0 && !fan.attached) || (angle >= 0 && !servo.attached)) return false;
    if (level >= 0) {
      if (!writeFan(level)) { fan.manual = false; return false; }
      fan.hold(now);
    }
    if (angle >= 0) {
      if (!writeServo(angle)) { servo.manual = false; return false; }
      servo.hold(now);
    }
    return true;
  }
  void automatic(const Sample& s, uint32_t now) {
    const linkage::Limits limits{TempFanOn, TempFanFull, SmokeFanOn, AirWindowOpen, ServoOpen, ServoClose, 40, 80};
    if (s.dhtOk) tempDemand = linkage::decide(true, s.temp, -1, -1, 0, limits).fanPct;
    if (s.smoke >= 0) smokeDemand = s.smoke >= SmokeFanOn ? limits.fanSmoke : 0;
    if (s.air >= 0) windowDemand = s.air >= AirWindowOpen ? ServoOpen : ServoClose;
    const int wantFan = tempDemand > smokeDemand ? tempDemand : smokeDemand;
    if (fan.attached && !fan.held(now) && wantFan != fan.confirmed) writeFan(wantFan);
    if (servo.attached && !servo.held(now) && windowDemand != servo.confirmed && (s.air >= 0 || servo.confirmed < 0)) writeServo(windowDemand);
  }
};
}  // namespace bmon
