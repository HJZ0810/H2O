#pragma once
#include <cmath>

// 联动决策核心：纯函数，不依赖 Arduino、不碰硬件。
// 固件里 OneNet::linkage() 调用它，主机侧 tools/linkage_test.cpp 也包含同一份头文件，
// 这样"模拟验证"验的就是真正烧进板子的那段逻辑，而不是另写一份仿制品。
//
// 阈值与任务 B 报告一致：
//   温度 ≥30°C 开风扇，30→40°C 由 40% 线性升到 100%
//   烟雾 ≥300 立即顶到 80%（排烟优先）
//   空气质量 ≥500 舵机开窗 90°，否则回 0°
namespace linkage {

struct Limits {
  float tempFanOn = 30.0f;
  float tempFanFull = 40.0f;
  int smokeFanOn = 300;
  int airWindowOpen = 500;
  int servoOpen = 90;
  int servoClose = 0;
  int fanMin = 40;   // 刚过温度阈值时的档位
  int fanSmoke = 80; // 烟雾触发的下限档位
};

struct Output {
  int fanPct;
  int servoDeg;
};

// temp 无效时传 dhtOk=false，此时不按温度开风扇
inline Output decide(bool dhtOk, float temp, int smokePpm, int airPpm,
                     int currentServo, const Limits& lim = Limits{}) {
  int fan = 0;
  if (dhtOk && temp >= lim.tempFanOn) {
    float k = (temp - lim.tempFanOn) / (lim.tempFanFull - lim.tempFanOn);
    if (k < 0) k = 0;
    if (k > 1) k = 1;
    fan = int(lim.fanMin + k * (100 - lim.fanMin));
  }
  // smokePpm < 0 表示该路不存在，不参与判断
  if (smokePpm >= 0 && smokePpm >= lim.smokeFanOn && fan < lim.fanSmoke) fan = lim.fanSmoke;

  int servo = currentServo;
  if (airPpm >= 0) servo = (airPpm >= lim.airWindowOpen) ? lim.servoOpen : lim.servoClose;

  return Output{fan, servo};
}

}  // namespace linkage
