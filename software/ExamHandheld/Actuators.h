#pragma once
#include <stdint.h>

class Actuators {
 public:
  void begin();
  void stop();
  void changeSpeed(int step);
  void turn(int step);
  bool ready() const { return ready_; }
  unsigned speedPercent() const;
  uint16_t pulseWidth() const { return pulse_; }
 private:
  bool ready_=false;
  unsigned speed_=0;
  uint16_t pulse_=1500;
};
