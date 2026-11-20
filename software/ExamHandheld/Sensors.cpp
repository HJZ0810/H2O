#include "Sensors.h"
#include "Board.h"
#include <ArduinoJson.h>
#include <time.h>
static AnalogSample readAnalog(int pin,bool enabled) {
  AnalogSample a;a.enabled=enabled;
  if(!enabled)return a;
  uint32_t raw=0,mv=0;
  for(int i=0;i<16;i++){raw+=analogRead(pin);mv+=analogReadMilliVolts(pin);}
  a.raw=raw/16;a.mv=mv/16;a.sampled=true;a.clipped=a.raw>=4090||a.mv>=3050;return a;
}
void Sensors::begin(const DeviceConfig& cfg) {
  mq[0].enabled=cfg.mq1;mq[1].enabled=cfg.mq2;
  analogReadResolution(12);
  for(int pin:{board::MQ1,board::MQ2}) {pinMode(pin,INPUT);analogSetPinAttenuation(pin,ADC_11db);}
  dht_.begin();lastDht_=millis();
}
void Sensors::tick() {
  uint32_t now=millis();
  if(uint32_t(now-lastAnalog_)>=500) {
    lastAnalog_=now;mq[0]=readAnalog(board::MQ1,mq[0].enabled);mq[1]=readAnalog(board::MQ2,mq[1].enabled);
    revision++;
  }
  if(uint32_t(now-lastDht_)>=2500) {
    lastDht_=now;float h=dht_.readHumidity(),t=dht_.readTemperature();
    dhtValid=isfinite(t)&&isfinite(h)&&t>=0&&t<=50&&h>=0&&h<=100;
    temperature=dhtValid?t:NAN;humidity=dhtValid?h:NAN;revision++;
  }
}
String Sensors::json() const {
  JsonDocument doc;doc["type"]="sensor_data";JsonObject d=doc["data"].to<JsonObject>();
  if(dhtValid) {d["temperature"]=temperature;d["humidity"]=humidity;}
  else {d["temperature"]=nullptr;d["humidity"]=nullptr;}
  d["dht_status"]=dhtValid?"ok":"unavailable";
  d["light"]=nullptr;
  d["light_status"]="unavailable";
  for(unsigned i=0;i<2;i++) {
    auto a=d[i?"mq2":"mq1"].to<JsonObject>();a["enabled"]=mq[i].enabled;
    a["valid"]=mq[i].valid();
    if(mq[i].sampled){a["raw"]=mq[i].raw;a["adc_mv"]=mq[i].mv;a["ao_mv"]=mq[i].outputMv();}
    else {a["raw"]=nullptr;a["adc_mv"]=nullptr;a["ao_mv"]=nullptr;}
    a["status"]=!mq[i].enabled?"disabled":!mq[i].sampled?"unavailable":mq[i].clipped?"clipped":"uncalibrated";
  }
  d["uptime_ms"]=millis();time_t now=time(nullptr);
  if(now>1700000000) d["timestamp"]=int64_t(now)*1000;
  d["firmware"]=board::FirmwareVersion;
  String out;serializeJson(doc,out);return out;
}
