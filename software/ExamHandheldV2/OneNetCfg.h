#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "OneNet.h"

// 任务 B 的接入层：从 NVS 读凭据、挂执行器、串口配置。
// 串口字符一律用数值比较（13/10），不用字符常量，避免转义被某一层吃掉。
namespace onenetcfg {

// 返回 true 表示这条命令已由本模块处理
inline bool handleLine(const String& cmd, OneNet& n, bool& act) {
  if (cmd.startsWith("on ")) {
    const String a = cmd.substring(3);
    const int i1 = a.indexOf(' ');
    const int i2 = a.lastIndexOf(' ');
    if (i1 <= 0 || i2 <= i1) {
      Serial.println("usage: on <pid> <dev> <key>");
      return true;
    }
    const String pid = a.substring(0, i1);
    const String dev = a.substring(i1 + 1, i2);
    const String key = a.substring(i2 + 1);
    n.configure(pid, dev, key);
    Preferences prefs;
    prefs.begin("zlc-config", false);
    prefs.putString("on_pid", pid);
    prefs.putString("on_dev", dev);
    prefs.putString("on_key", key);
    prefs.end();
    Serial.print("[onenet] saved pid=");
    Serial.print(pid);
    Serial.print(" dev=");
    Serial.print(dev);
    Serial.print(" keylen=");
    Serial.println(key.length());
    return true;
  }
  if (cmd == "onshow") {
    Serial.print("[onenet] configured=");
    Serial.print(n.configured() ? 1 : 0);
    Serial.print(" active=");
    Serial.print(act ? 1 : 0);
    Serial.print(" conn=");
    Serial.print(n.connected() ? 1 : 0);
    Serial.print(" down=");
    Serial.println(n.downlinkCount());
    return true;
  }
  if (cmd == "onon") { act = true; Serial.println("[onenet] monitor on"); return true; }
  if (cmd == "onoff") { act = false; Serial.println("[onenet] monitor off"); return true; }
  return false;
}

inline void loadFromNvs(OneNet& n, bool& act, Actuators& actr) {
  Preferences prefs;
  prefs.begin("zlc-config", true);
  n.configure(prefs.getString("on_pid", ""), prefs.getString("on_dev", ""),
              prefs.getString("on_key", ""), prefs.getString("on_host", "mqtts.heclouds.com"));
  prefs.end();
  n.attach(&actr);
  act = n.configured();  // 默认就连，不需要用户去菜单里开
  Serial.print("[onenet] configured=");
  Serial.println(n.configured() ? 1 : 0);
}

}  // namespace onenetcfg
