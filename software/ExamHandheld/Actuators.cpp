#include "Actuators.h"
#include "Board.h"
#include <Arduino.h>

namespace {
struct BridgePins {
  int pwm, in1, in2;
};

constexpr BridgePins MotorA{board::MotorPwm, board::MotorAin1, board::MotorAin2};
constexpr BridgePins MotorB{board::MotorPwmB, board::MotorBin1, board::MotorBin2};
constexpr unsigned Speeds[] = {0, 40, 60, 80, 100};
constexpr int MaxSpeedIndex = sizeof(Speeds) / sizeof(Speeds[0]) - 1;
constexpr unsigned MotorBits = 8;
constexpr unsigned FullDuty = (1U << MotorBits) - 1;
constexpr unsigned ServoBits = 14;
constexpr unsigned ServoPeriodUs = 20000;
constexpr int ServoMinUs = 1200, ServoCenterUs = 1500, ServoMaxUs = 1800;
constexpr int ServoStepUs = 50;

bool setMotor(const BridgePins& pins, unsigned speed) {
  digitalWrite(pins.in2, LOW);
  if (!speed) {
    // PWM high and both inputs low selects high-impedance STOP.
    digitalWrite(pins.in1, LOW);
    return ledcWrite(pins.pwm, FullDuty);
  }
  if (!ledcWrite(pins.pwm, speed * FullDuty / 100)) return false;
  digitalWrite(pins.in1, HIGH);
  return true;
}
}

void Actuators::begin() {
  for (int pin : {MotorA.pwm, MotorA.in1, MotorA.in2,
                  MotorB.pwm, MotorB.in1, MotorB.in2, board::ServoPwm}) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
  }
  const bool aOk = ledcAttachChannel(MotorA.pwm, 20000, MotorBits, 0);
  const bool bOk = ledcAttachChannel(MotorB.pwm, 20000, MotorBits, 1);
  const bool servoOk = ledcAttachChannel(board::ServoPwm, 50, ServoBits, 2);
  ready_ = aOk && bOk && servoOk;
  stop();
}

unsigned Actuators::speedPercent(bool channelB) const {
  return Speeds[channelB ? speedB_ : speedA_];
}

void Actuators::stop() {
  speedA_ = speedB_ = 0;
  pulse_ = 0;
  digitalWrite(MotorA.in1, LOW);
  digitalWrite(MotorA.in2, LOW);
  digitalWrite(MotorB.in1, LOW);
  digitalWrite(MotorB.in2, LOW);
  if (ready_) {
    const bool aOk = setMotor(MotorA, 0);
    const bool bOk = setMotor(MotorB, 0);
    const bool servoOk = ledcWrite(board::ServoPwm, 0);
    ready_ = aOk && bOk && servoOk;
  }
}

void Actuators::changeSpeed(int step) {
  if (!ready_) return;
  auto& speed = channelB_ ? speedB_ : speedA_;
  const int next = constrain(int(speed) + step, 0, MaxSpeedIndex);
  if (!setMotor(channelB_ ? MotorB : MotorA, Speeds[next])) {
    stop();
    ready_ = false;
    return;
  }
  speed = next;
}

void Actuators::turn(int step) {
  if (!ready_) return;
  const int current = pulse_ ? pulse_ : ServoCenterUs;
  const int next = constrain(current + step * ServoStepUs, ServoMinUs, ServoMaxUs);
  const auto duty = (uint32_t(next) << ServoBits) / ServoPeriodUs;
  if (ledcWrite(board::ServoPwm, duty)) pulse_ = next;
}
