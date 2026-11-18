#pragma once
#include <string>
#include <cstdint>
struct PortalFields {
  std::string mac,token,ssid,group,name,host;
  uint16_t port=1883;
  unsigned rotation=1,panel=0;
  bool mq1=false,mq2=false,hasMqttPassword=false;
};
std::string buildPortalHtml(const PortalFields& fields);
