#pragma once

#include <stdint.h>

class Actuators {
 public:
  void begin();
  void stop();
  void changeSpeed(int step);
  void turn(int step);
  void selectNextChannel() { channelB_ = !channelB_; }
  bool channelB() const { return channelB_; }
  bool ready() const { return ready_; }
  unsigned speedPercent(bool channelB) const;
  uint16_t pulseWidth() const { return pulse_; }

 private:
  bool ready_ = false;
  bool channelB_ = false;
  uint8_t speedA_ = 0, speedB_ = 0;
  uint16_t pulse_ = 0;
};
