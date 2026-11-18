#pragma once
#include <Arduino.h>
#include <DHT.h>
#include "Config.h"
struct AnalogSample {int raw=0,mv=0; bool enabled=false,clipped=false;};
class Sensors {
 public:
  float temperature=NAN,humidity=NAN,lightPercent=0;
  int lightRaw=0,lightMv=0;
  AnalogSample mq[2];
  bool dhtValid=false,lightClipped=false;
  unsigned revision=0;
  void begin(const DeviceConfig& config);
  void tick();
  String json() const;
 private:
  DHT dht_{5,DHT11};
  uint32_t lastAnalog_=0,lastDht_=0;
};
