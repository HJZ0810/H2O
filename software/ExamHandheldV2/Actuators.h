#pragma once

#include <stdint.h>

struct ActuatorCommand {
  enum Field : uint8_t { MotorA = 1, MotorB = 2, ServoPosition = 4, ServoEnabled = 8 };
  uint8_t fields = 0;
  int motorAPct = 0, motorBPct = 0, servoPosition = 0;
  bool servoEnabled = false;
};

class Actuators {
 public:
  void begin();
  void stop();
  void changeSpeed(int step);
  void turn(int step);
  bool apply(const ActuatorCommand& command);
  void selectNextChannel() { channelB_ = !channelB_; }
  bool channelB() const { return channelB_; }
  bool ready() const { return ready_; }
  unsigned speedPercent(bool channelB) const;
  int servoPosition() const { return servoPosition_; }
  uint16_t pulseWidth() const { return pulse_; }

 private:
  bool ready_ = false;
  bool channelB_ = false;
  uint8_t speedA_ = 0, speedB_ = 0;
  int servoPosition_ = 0;
  uint16_t pulse_ = 0;
  bool setServo(int position, bool enabled);
  bool fail();
};
