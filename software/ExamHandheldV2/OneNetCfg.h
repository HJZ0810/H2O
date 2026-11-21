#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "OneNet.h"

// 任务 B 的接入层：从 NVS 读凭据、挂执行器、串口配置、开机对时。
// 单独成文件，一是让 ExamHandheldV2.ino 只留一行调用，二是避开在
// shell 里拼转义字符导致字符串被换行打断（见踩坑记录第 18 条）。
namespace onenetcfg {

inline void loadFromNvs(OneNet& n, bool& act, Actuators& actr) {
  Preferences prefs;
  prefs.begin("zlc-config", true);
  n.configure(prefs.getString("on_pid", ""), prefs.getString("on_dev", ""),
              prefs.getString("on_key", ""), prefs.getString("on_host", "mqtts.heclouds.com"));
  prefs.end();
  n.attach(&actr);
  Serial.print("[onenet] configured=");
  Serial.println(n.configured() ? 1 : 0);
}

// 串口命令：on <pid> <dev> <key> / onshow / onon / onoff
// 凭据不写进代码，与任务 A 的组号一样放 NVS。
inline void poll(OneNet& n, bool& act) {
  static String line;
  while (Serial.available()) {
    const char ch = char(Serial.read());
    if (ch == '\r') continue;
    if (ch != '\n') {
      if (line.length() < 220) line += ch;
      continue;
    }
    line.trim();
    if (line.isEmpty()) continue;
    const String cmd = line;
    line = "";
    if (cmd.startsWith("on ")) {
      const String a = cmd.substring(3);
      const int i1 = a.indexOf(' ');
      const int i2 = a.lastIndexOf(' ');
      if (i1 <= 0 || i2 <= i1) {
        Serial.println("usage: on <pid> <dev> <key>");
        continue;
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
    } else if (cmd == "onshow") {
      Serial.print("[onenet] configured=");
      Serial.print(n.configured() ? 1 : 0);
      Serial.print(" active=");
      Serial.print(act ? 1 : 0);
      Serial.print(" conn=");
      Serial.print(n.connected() ? 1 : 0);
      Serial.print(" down=");
      Serial.println(n.downlinkCount());
    } else if (cmd == "onon") {
      act = true;
      Serial.println("[onenet] monitor on");
    } else if (cmd == "onoff") {
      act = false;
      Serial.println("[onenet] monitor off");
    }
  }
}

}  // namespace onenetcfg
