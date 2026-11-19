#include "Actuators.h"
#include "Board.h"
#include <Arduino.h>

namespace {
constexpr unsigned Speeds[]={0,40,60,80,100};
constexpr unsigned ServoBits=14;
constexpr unsigned MotorBits=8;
constexpr unsigned ServoPeriodUs=20000;
// Limit travel during first assembly. Microseconds describe the command,
// since MG90S variants do not all map pulse width to the same angle.
constexpr int ServoMinUs=1200;
constexpr int ServoMaxUs=1800;
constexpr int ServoStepUs=50;
}

void Actuators::begin() {
  for (int pin : {board::MotorStandby,board::MotorAin1,board::MotorAin2,
                  board::MotorPwm,board::ServoPwm}) {
    digitalWrite(pin,LOW);
    pinMode(pin,OUTPUT);
  }
  // Channels 0 and 2 use separate timers on the ESP32-S3 LEDC peripheral.
  const bool motor=ledcAttachChannel(board::MotorPwm,20000,MotorBits,0);
  const bool servo=ledcAttachChannel(board::ServoPwm,50,ServoBits,2);
  ready_=motor && servo;
  if (motor) ledcWrite(board::MotorPwm,0);
  if (servo) ledcWrite(board::ServoPwm,0);
}

unsigned Actuators::speedPercent() const { return Speeds[speed_]; }

void Actuators::stop() {
  digitalWrite(board::MotorStandby,LOW);
  digitalWrite(board::MotorAin1,LOW);
  digitalWrite(board::MotorAin2,LOW);
  speed_=0;
  if (ready_) {
    ledcWrite(board::MotorPwm,0);
    ledcWrite(board::ServoPwm,0);
  }
}

void Actuators::changeSpeed(int step) {
  if (!ready_) return;
  const int next=constrain(int(speed_)+step,0,int(sizeof(Speeds)/sizeof(Speeds[0]))-1);
  if (next==0) {
    digitalWrite(board::MotorStandby,LOW);
    ledcWrite(board::MotorPwm,0);
    speed_=0;
    return;
  }
  digitalWrite(board::MotorAin1,HIGH);
  digitalWrite(board::MotorAin2,LOW);
  if (!ledcWrite(board::MotorPwm,Speeds[next]*((1U<<MotorBits)-1)/100)) { stop(); return; }
  speed_=next;
  digitalWrite(board::MotorStandby,HIGH);
}

void Actuators::turn(int step) {
  if (!ready_) return;
  const auto next=constrain(int(pulse_)+step*ServoStepUs,ServoMinUs,ServoMaxUs);
  if (ledcWrite(board::ServoPwm,(uint32_t(next)*(1U<<ServoBits))/ServoPeriodUs)) pulse_=next;
}
