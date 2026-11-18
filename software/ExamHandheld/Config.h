#pragma once
#include <Arduino.h>
#include <WebServer.h>
struct DeviceConfig {
  String ssid, wifiPassword, group, mqttPassword, name="手柄", host="124.70.71.221";
  uint16_t port=1883;
  uint8_t rotation=1, panel=0;
  bool mq1=false,mq2=false;
  bool usable() const {return !ssid.isEmpty() && !group.isEmpty() && !mqttPassword.isEmpty();}
};
class Configuration {
 public:
  DeviceConfig value;
  bool portalActive=false;
  String apName,apPassword,message;
  void load();
  bool openPortal();
  void tick();
 private:
  WebServer server_{80};
  String token_;
  uint32_t rebootAt_=0;
  void save();
  void page();
  void accept();
};
