#pragma once
#include <stdint.h>
#include <math.h>

// 任务 B · 物联监测控制系统
// 抽签配置（2026-11-18，24级物联网工程4班 一氧化二氢）：
//   传感器   S01 DHT11 温湿度 / S05 MQ-2 烟雾可燃气体 / S06 MQ-135 空气质量
//   执行机构 A05 直流小马达+风扇叶 / A04 SG90 舵机
//   云平台   C01 OneNET（中国移动）
//
// 两套板型共用同一份业务代码：
//   R9 自研板  —— 手柄板 + 执行板，10 芯 IDC 直连，有功率级
//   验证板     —— 只有 DHT11 与一路模拟量，无 MQ 传感器、无功率级
// 引脚以 Board.h 的宏为准，这里只定义任务 B 需要的量程与阈值。
#if defined(BOARD_VERIFY)
namespace bmon {
constexpr bool kVerify = true;
constexpr int Dht = 8;       // 板载 DHT11
constexpr int Mq2Pin = 1;    // 验证板无 MQ-2，此脚为光敏电阻，仅用于验证链路
constexpr int Mq135Pin = -1; // 不存在
constexpr int FanPwm = 4;    // 映射到板载 LED，亮度随占空比
constexpr int FanIn1 = -1, FanIn2 = -1;
constexpr int ServoPwm = 6;  // 映射到板载 LED
constexpr int LedPwmB = 5;   // 预留：第二路指示灯
constexpr const char* BoardName = "verify-board";
}  // namespace bmon
#else
namespace bmon {
constexpr bool kVerify = false;
constexpr int Dht = 5;
constexpr int Mq2Pin = 1;    // ADC1（GPIO1~10），联网可用
constexpr int Mq135Pin = 4;  // ADC1
constexpr int FanPwm = 41, FanIn1 = 40, FanIn2 = 39;  // 风扇，TB6612 类驱动
constexpr int ServoPwm = 42;
constexpr int LedPwmB = 48;
constexpr const char* BoardName = "YD-25.40-R10";
}  // namespace bmon
#endif

namespace bmon {

// ---- 量程与阈值 ----
// MQ 系列在 3.3V 量程下按线性近似折算，只用于趋势与阈值判断，
// 不是标定过的 ppm。报告里必须写明这一点（老师明确要求标定说明）。
constexpr const char* FirmwareVersion = "10.0.0-rc.2";

// 联动阈值（ESP32 端执行）
constexpr float TempFanOn = 30.0f;    // 温度 ≥30°C 开风扇
constexpr float TempFanFull = 40.0f;  // 40°C 时风扇满转
constexpr int SmokeFanOn = 300;       // 烟雾 ≥300 立即开风扇排烟
constexpr int AirWindowOpen = 500;    // 空气质量 ≥500 舵机开到 90°（模拟开窗）
constexpr int ServoOpen = 90, ServoClose = 0;

// 上报周期：要求 ≥1 次/30s，这里 10s，留足余量
constexpr uint32_t ReportIntervalMs = 10000;

// 云端手动控制后的静默期：期间不被自动联动覆盖，避免指令打架
constexpr uint32_t ManualHoldMs = 30000;

// 一次采样结果。放在头文件里，避免 Arduino 预处理器把函数原型插到结构体之前。
struct Sample {
  float temp = NAN, humi = NAN;
  int smoke = -1, air = -1;  // -1 表示该路不存在
  bool dhtOk = false;
};

}  // namespace bmon
