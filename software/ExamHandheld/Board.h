#pragma once
#include <stdint.h>
namespace board {
constexpr int TFT_CS=15,TFT_DC=2,TFT_RST=21,TFT_MOSI=13,TFT_SCK=14;
constexpr int DHT=5,MQ1=1,MQ2=4,LDR=6;
constexpr uint8_t Keys[]={7,8,9,10,11,12,16,17,18};
constexpr float MqDivider=33.0f/(22.0f+33.0f);
constexpr const char* FirmwareVersion="1.1.0-r3.1";
}
