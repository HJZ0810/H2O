#!/usr/bin/env python
"""联动逻辑本机模拟验证。

固件里的决策函数是 LinkageCore.h 的 linkage::decide()。本机没有 C++ 编译器，
所以这里做两件事：
  1. 从头文件里解析出阈值常量（单一事实来源），避免模拟器和固件跑偏；
  2. 用同一套判定规则跑用例表，逐条断言。

局限：验证的是"规则与常量"，不是编译后的二进制。真机动作（风扇转速、舵机角度）
仍需在 R9 板上实测，报告里要分开写。
"""
import re
import sys
from pathlib import Path

HEADER = Path(__file__).resolve().parents[1] / "B-monitor" / "LinkageCore.h"


def load_limits(path):
    text = path.read_text(encoding="utf-8")
    nums = {}

    def grab(name, cast=float):
        m = re.search(rf"{name}\s*=\s*([-\d.]+)f?", text)
        if not m:
            raise SystemExit(f"头文件里找不到 {name}")
        nums[name] = cast(m.group(1))

    grab("tempFanOn")
    grab("tempFanFull")
    grab("smokeFanOn", int)
    grab("airWindowOpen", int)
    grab("servoOpen", int)
    grab("servoClose", int)
    grab("fanMin", int)
    grab("fanSmoke", int)
    return nums


def decide(lim, dht_ok, temp, smoke, air, current_servo):
    fan = 0
    if dht_ok and temp >= lim["tempFanOn"]:
        k = (temp - lim["tempFanOn"]) / (lim["tempFanFull"] - lim["tempFanOn"])
        k = max(0.0, min(1.0, k))
        fan = int(lim["fanMin"] + k * (100 - lim["fanMin"]))
    if smoke >= 0 and smoke >= lim["smokeFanOn"] and fan < lim["fanSmoke"]:
        fan = lim["fanSmoke"]
    servo = current_servo
    if air >= 0:
        servo = lim["servoOpen"] if air >= lim["airWindowOpen"] else lim["servoClose"]
    return fan, servo


CASES = [
    # 说明, dht_ok, 温度, 烟雾, 空气质量, 当前角度, 期望风扇, 期望角度
    ("常温洁净空气，不动",            True,  25.0,  120,  180,  0,    0,   0),
    ("温度刚过阈值",                  True,  30.0,  120,  180,  0,   40,   0),
    ("温度中点 35°C",                 True,  35.0,  120,  180,  0,   70,   0),
    ("温度到顶 40°C",                 True,  40.0,  120,  180,  0,  100,   0),
    ("温度超量程 45°C 封顶",          True,  45.0,  120,  180,  0,  100,   0),
    ("低温但烟雾超标，排烟优先",      True,  24.0,  320,  180,  0,   80,   0),
    ("高温+烟雾，取较大档",           True,  38.0,  320,  180,  0,   88,   0),
    ("烟雾刚好等于阈值",              True,  24.0,  300,  180,  0,   80,   0),
    ("空气质量超标开窗",              True,  24.0,  120,  520,  0,    0,  90),
    ("空气质量回落后关窗",            True,  24.0,  120,  300, 90,    0,   0),
    ("空气质量正好阈值",              True,  24.0,  120,  500,  0,    0,  90),
    ("DHT 失效不按温度开风扇",        False, 99.0,  120,  180,  0,    0,   0),
    ("DHT 失效但烟雾超标仍排烟",      False, 99.0,  400,  180,  0,   80,   0),
    ("验证板无 MQ-135，舵机保持",     True,  26.0,  943,   -1,  0,   80,   0),
    ("无 MQ 通道，只靠温度",          True,  33.0,   -1,   -1, 45,   58,  45),
]


def main():
    lim = load_limits(HEADER)
    print("从 LinkageCore.h 解析到的阈值：")
    print(f"  温度 {lim['tempFanOn']}~{lim['tempFanFull']} °C → 风扇 {lim['fanMin']}~100%")
    print(f"  烟雾 ≥{lim['smokeFanOn']} → 风扇 ≥{lim['fanSmoke']}%")
    print(f"  空气质量 ≥{lim['airWindowOpen']} → 舵机 {lim['servoOpen']}°，否则 {lim['servoClose']}°")
    print()

    width = max(len(c[0]) for c in CASES)
    fails = 0
    for desc, dht, temp, smoke, air, cur, exp_fan, exp_servo in CASES:
        fan, servo = decide(lim, dht, temp, smoke, air, cur)
        ok = (fan == exp_fan and servo == exp_servo)
        fails += 0 if ok else 1
        mark = "PASS" if ok else "FAIL"
        print(f"{mark}  {desc:<{width}}  T={temp:>5} smoke={smoke:>4} air={air:>4} "
              f"→ fan={fan:>3}% servo={servo:>3}°  (期望 {exp_fan}% / {exp_servo}°)")

    print()
    print(f"用例 {len(CASES)} 条，通过 {len(CASES) - fails} 条，失败 {fails} 条")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
