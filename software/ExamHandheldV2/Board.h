#pragma once
#include <stdint.h>

// 两套板型共用同一份业务代码，仅在引脚与可选外设上分叉。
// 自研板 = R9.1.3 双板（手柄板 + 执行板，10 芯 IDC 直连）
// 验证板 = 「ESP32S3 功能板 TFT版」，同型号 SoC、同尺寸屏类、板载 DHT11 与 6 键
#if defined(BOARD_VERIFY)

namespace board {
constexpr bool kVerifyBoard = true;

// TFT：1.8" 128x160，8 脚 SPI。实测确认（2026-11-21）：
// SCL=IO38, SDA=IO21, CS=IO10, DC=IO11, RES=IO12, BLK=IO9，与原理图 U2 一致。
// 注意：SPI 只允许初始化一次；重复 begin() 不会切换引脚，会造成误判。
constexpr int TFT_SCK = 38, TFT_MOSI = 21, TFT_CS = 10, TFT_DC = 11, TFT_RST = 12, TFT_BL = 9;
constexpr int TFT_DEFAULT_W = 128, TFT_DEFAULT_H = 160;
constexpr int TFT_DEFAULT_ROT = 1;
constexpr int TFT_CTRL = 0;  // ST7735

constexpr int DHT = 8;
// 板上无 MQ-2 / MQ-135；IO1 为光敏电阻，作为第一路模拟量输入
constexpr int MQ1 = 1, MQ2 = -1;
constexpr bool kHasMq = false;
constexpr float MqDivider = 1.0f;

// 6 个轻触键 + BOOT 键。BOOT(IO0) 补足第 7 个功能键，上电时不要按住。
enum Key : uint8_t { Test, Back, Up, Down, Left, Right, Confirm, KeyCount };
constexpr uint8_t Keys[KeyCount] = {0, 14, 15, 16, 17, 18, 13};

// 无功率级：A/B 与舵机映射到板载 LED，亮度随占空比变化，用于直观确认输出
constexpr int ServoPwm = 6, MotorPwm = 4, MotorAin1 = -1, MotorAin2 = -1;
constexpr int MotorPwmB = 5, MotorBin1 = -1, MotorBin2 = -1;
constexpr bool kHasPowerStage = false;

constexpr const char* HanOutlineUrl = "http://124.70.71.221/r9-font/outline-v1/han-outline.bin";
constexpr const char* FirmwareVersion = "2.0.1-r9-ui";
constexpr const char* BoardName = "verify-board";

}  // namespace board

#else

namespace board {
constexpr bool kVerifyBoard = false;

constexpr int TFT_CS = 15, TFT_DC = 2, TFT_RST = 21, TFT_MOSI = 13, TFT_SCK = 14, TFT_BL = -1;
constexpr int TFT_DEFAULT_W = 128, TFT_DEFAULT_H = 160;
constexpr int TFT_DEFAULT_ROT = 1;
constexpr int TFT_CTRL = 0;  // ST7735

constexpr int DHT = 5, MQ1 = 1, MQ2 = 4;
constexpr bool kHasMq = true;
constexpr float MqDivider = 33.0f / (22.0f + 33.0f);

enum Key : uint8_t { Test, Back, Up, Down, Left, Right, Confirm, KeyCount };
constexpr uint8_t Keys[KeyCount] = {7, 8, 11, 12, 16, 17, 18};

constexpr int ServoPwm = 42, MotorPwm = 41, MotorAin1 = 40, MotorAin2 = 39;
constexpr int MotorPwmB = 48, MotorBin1 = 38, MotorBin2 = 43;
constexpr bool kHasPowerStage = true;

constexpr const char* HanOutlineUrl = "http://124.70.71.221/r9-font/outline-v1/han-outline.bin";
constexpr const char* FirmwareVersion = "2.0.1-r9-ui";
constexpr const char* BoardName = "r9.1.3";

}  // namespace board

#endif
