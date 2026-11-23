#pragma once
#include <WiFi.h>
#if __has_include("OneNetTrust.h")
#include "OneNetTrust.h"
#include <WiFiClientSecure.h>
namespace onetransport {
using Client = WiFiClientSecure;
constexpr uint16_t Port = 8883;
inline void prepare(Client& client) { client.setCACert(H2O_ONENET_CA); }
}
#else
namespace onetransport {
using Client = WiFiClient;
constexpr uint16_t Port = 1883;
inline void prepare(Client&) {}
}
#endif
