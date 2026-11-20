#pragma once
#include <string>
#include <cstdint>
struct PortalFields {
  std::string mac,token,ssid,group,name,host,cloudHost;
  uint16_t port=1883;
  unsigned rotation=1,panel=0;
  bool mq1=true,mq2=true,hasCloudCredentials=false,hasDemoAnswers=false;
};
std::string buildPortalHtml(const PortalFields& fields);
