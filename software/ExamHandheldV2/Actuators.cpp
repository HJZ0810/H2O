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
constexpr unsigned MotorBits = 8;
constexpr unsigned FullDuty = (1U << MotorBits) - 1;
constexpr unsigned ServoBits = 14;
constexpr unsigned ServoPeriodUs = 20000;
constexpr int ServoCenterUs = 1500;
constexpr int ServoUsPerPercent = 3;
constexpr int ServoStep = 17;

bool setMotor(const BridgePins& pins, unsigned speed) {
  if (pins.in2 >= 0) digitalWrite(pins.in2, LOW);
  if (!speed) {
    // PWM high and both inputs low selects high-impedance STOP.
    if (pins.in1 >= 0) digitalWrite(pins.in1, LOW);
    return ledcWrite(pins.pwm, pins.in1 < 0 ? 0 : FullDuty);
  }
  if (!ledcWrite(pins.pwm, speed * FullDuty / 100)) return false;
  if (pins.in1 >= 0) digitalWrite(pins.in1, HIGH);
  return true;
}
}

void Actuators::begin() {
  for (int pin : {MotorA.pwm, MotorA.in1, MotorA.in2,
                  MotorB.pwm, MotorB.in1, MotorB.in2, board::ServoPwm}) {
    if (pin < 0) continue;
#ifdef H2O_KEEP_UART0
    if (pin == 43) continue;  // 调试构建：GPIO43 与 UART0 TX 复用，保留串口，B 路 BIN2 暂不驱动
#endif
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
  return channelB ? speedB_ : speedA_;
}

void Actuators::stop() {
  speedA_ = speedB_ = 0;
  servoPosition_ = 0;
  pulse_ = 0;
  if (MotorA.in1 >= 0) digitalWrite(MotorA.in1, LOW);
  if (MotorA.in2 >= 0) digitalWrite(MotorA.in2, LOW);
  if (MotorB.in1 >= 0) digitalWrite(MotorB.in1, LOW);
#ifndef H2O_KEEP_UART0
  if (MotorB.in2 >= 0) digitalWrite(MotorB.in2, LOW);
#endif
  if (ready_) {
    const bool aOk = setMotor(MotorA, 0);
    const bool bOk = setMotor(MotorB, 0);
    const bool servoOk = ledcWrite(board::ServoPwm, 0);
    if (!servoOk) {
      ledcDetach(board::ServoPwm);
      pinMode(board::ServoPwm, OUTPUT);
      digitalWrite(board::ServoPwm, LOW);
    }
    ready_ = aOk && bOk && servoOk;
  }
}

void Actuators::changeSpeed(int step) {
  const auto current = speedPercent(channelB_);
  auto next = current;
  if (step > 0) {
    for (unsigned speed : Speeds) {
      if (speed > current) { next = speed; break; }
    }
  } else if (step < 0) {
    for (unsigned speed : Speeds) {
      if (speed >= current) break;
      next = speed;
    }
  }
  ActuatorCommand command;
  if (channelB_) {
    command.fields = ActuatorCommand::MotorB;
    command.motorBPct = next;
  } else {
    command.fields = ActuatorCommand::MotorA;
    command.motorAPct = next;
  }
  apply(command);
}

void Actuators::turn(int step) {
  ActuatorCommand command;
  command.fields = ActuatorCommand::ServoPosition;
  command.servoPosition = constrain(servoPosition_ + step * ServoStep, -100, 100);
  apply(command);
}

bool Actuators::fail() {
  stop();
  ready_ = false;
  return false;
}

bool Actuators::setServo(int position, bool enabled) {
  const int pulse = enabled ? ServoCenterUs + position * ServoUsPerPercent : 0;
  const auto duty = (uint32_t(pulse) << ServoBits) / ServoPeriodUs;
  if (!ledcWrite(board::ServoPwm, duty)) return false;
  pulse_ = pulse;
  servoPosition_ = position;
  return true;
}

bool Actuators::apply(const ActuatorCommand& command) {
  if (!ready_) return false;
  if ((command.fields & ActuatorCommand::ServoPosition) && (command.servoPosition < -100 || command.servoPosition > 100)) return false;
  if ((command.fields & ActuatorCommand::MotorA) && (command.motorAPct < 0 || command.motorAPct > 100)) return false;
  if ((command.fields & ActuatorCommand::MotorB) && (command.motorBPct < 0 || command.motorBPct > 100)) return false;
  if ((command.fields & ActuatorCommand::MotorA) && command.motorAPct != speedA_) {
    if (!setMotor(MotorA, command.motorAPct)) return fail();
    speedA_ = command.motorAPct;
  }
  if ((command.fields & ActuatorCommand::MotorB) && command.motorBPct != speedB_) {
    if (!setMotor(MotorB, command.motorBPct)) return fail();
    speedB_ = command.motorBPct;
  }
  const bool hasPosition = command.fields & ActuatorCommand::ServoPosition;
  const bool hasEnable = command.fields & ActuatorCommand::ServoEnabled;
  if (hasPosition || hasEnable) {
    const int position = hasPosition ? command.servoPosition : servoPosition_;
    const bool enabled = hasEnable ? command.servoEnabled : (hasPosition || pulse_ != 0);
    if (!setServo(position, enabled)) return fail();
  }
  return true;
}
