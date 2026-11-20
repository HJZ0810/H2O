#pragma once
#include <stdint.h>
namespace board {
constexpr int TFT_CS=15,TFT_DC=2,TFT_RST=21,TFT_MOSI=13,TFT_SCK=14;
constexpr int DHT=5,MQ1=1,MQ2=4;
enum Key : uint8_t { Test, Back, Up, Down, Left, Right, Confirm, KeyCount };
constexpr uint8_t Keys[KeyCount]={7,8,11,12,16,17,18};
constexpr int ServoPwm=42, MotorPwm=41, MotorAin1=40, MotorAin2=39;
constexpr int MotorPwmB=48, MotorBin1=38, MotorBin2=43;
constexpr float MqDivider=33.0f/(22.0f+33.0f);
constexpr const char* FirmwareVersion="1.2.2-r9-10p";
}
