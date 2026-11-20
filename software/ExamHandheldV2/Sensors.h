#pragma once
#include <Arduino.h>
#include <DHT.h>
#include "Config.h"
#include "Board.h"
struct AnalogSample {
  int raw=0,mv=0;
  bool enabled=false,sampled=false,clipped=false;
  bool valid() const {return enabled && sampled && !clipped;}
  int outputMv() const {return lroundf(mv/board::MqDivider);}
};
class Sensors {
 public:
  float temperature=NAN,humidity=NAN;
  AnalogSample mq[2];
  bool dhtValid=false;
  unsigned revision=0;
  void begin(const DeviceConfig& config);
  void tick();
  String json() const;
 private:
  DHT dht_{board::DHT,DHT11};
  uint32_t lastAnalog_=0,lastDht_=0;
};
