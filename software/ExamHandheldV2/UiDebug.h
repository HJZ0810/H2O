#pragma once
#include <mbedtls/base64.h>
#include <Preferences.h>

// 串口用于屏幕诊断和设备配置。考试按键只从 GPIO 读取。
//
// 摄像头拍出来的屏太小、有摩尔纹，判不清字形细节；直接把 framebuf 逐像素导出来，
// 才能确定"这一行到底是字被裁了，还是相机糊了"。
//
// 命令：
//   page                                   打印当前 view / phase / menuIndex
//   shot                                   导出 160x128 RGB565，base64 分行
//
// 在主程序定义屏幕状态和 localStop() 之后包含本文件。

static const char* viewName(int v) {
  switch (v) {
    case 0: return "Menu";
    case 1: return "Question";
    case 2: return "Story";
    case 3: return "Judge";
    case 4: return "Finished";
    case 5: return "Recovery";
    case 6: return "Sensor";
    case 7: return "Info";
    case 8: return "Setup";
    case 9: return "Fan";
    case 10: return "Monitor";
    default: return "?";
  }
}

static void dumpShot() {
  const int w = panel.width();
  const int h = panel.height();
  const unsigned char* p = reinterpret_cast<const unsigned char*>(framebuf);
  const size_t total = size_t(w) * size_t(h) * 2;
  Serial.print("SHOT ");
  Serial.print(w);
  Serial.print(" ");
  Serial.println(h);
  size_t i = 0;
  while (i < total) {
    size_t chunk = (total - i) > 120 ? 120 : (total - i);  // 120 是 3 的倍数，便于分段解码
    unsigned char out[200];
    size_t outLen = 0;
    if (mbedtls_base64_encode(out, sizeof(out), &outLen, p + i, chunk) != 0) break;
    Serial.print("D ");
    Serial.write(out, outLen);
    Serial.println();
    i += chunk;
  }
  Serial.println("END");
}

static unsigned long fbChecksum() {
  const uint16_t* p = framebuf;
  const size_t n = size_t(panel.width()) * size_t(panel.height());
  unsigned long h = 2166136261UL;
  for (size_t i = 0; i < n; i++) {
    h ^= uint16_t(p[i]);
    h *= 16777619UL;
  }
  return h;
}

static void pollUiDebug() {
  static String buf;
  static bool overflow = false;
  while (Serial.available()) {
    const int ch = Serial.read();
    if (ch == 13) continue;
    if (ch != 10) {
      if (buf.length() < 300) buf += char(ch); else overflow = true;
      continue;
    }
    if (overflow) { overflow = false; buf = ""; Serial.println("command too long"); continue; }
    buf.trim();
    const String cmd = buf;
    buf = "";
    if (cmd.isEmpty()) continue;

    if (cmd == "page") {
      Serial.print("[ui] view=");
      Serial.print(viewName(int(view)));
      Serial.print(" phase=");
      Serial.print(int(session.phase));
      Serial.print(" menu=");
      Serial.print(menuIndex);
      Serial.print(" list=");
      Serial.print(view == View::Menu ? 6 : 0);
      Serial.print(" fb=");
      Serial.println(fbChecksum());
      continue;
    }
    if (cmd == "shot") { dumpShot(); continue; }

    // view <n>：直接跳到某个界面，绕过按键路径，便于远程核对布局
    if (cmd.startsWith("view ")) {
      const int v = cmd.substring(5).toInt();
      if (v >= 0 && v <= 10) {
        switchView(static_cast<View>(v));
        Serial.printf("[ui] view=%s\n", viewName(v));
      }
      continue;
    }
    // key <n>：模拟一次短按，枚举同 board::Key（0 测试 1 返回 2 上 3 下 4 左 5 右 6 确认）
    if (cmd.startsWith("key ")) {
      const int k = cmd.substring(4).toInt();
      if (k >= 0 && k < board::KeyCount) {
        handleKey(static_cast<board::Key>(k), Press::Short);
        Serial.printf("[ui] key=%d scroll=%d/%d\n", k, pageScroll, pageScrollMax);
      }
      continue;
    }

    if (cmd == "sensor") {
      Serial.print("[sensor] ");
      Serial.println(sensors.json());
      continue;
    }
    if (cmd == "cfg") {
      Serial.printf("[cfg] ssid=%s wifiPass=%s grp=%s pass=%s mac=%s ip=%s wifi=%d rssi=%d online=%d ready=%d\n",
                    configuration.value.ssid.c_str(), configuration.value.wifiPassword.c_str(),
                    configuration.value.group.c_str(),
                    configuration.value.mqttPassword.c_str(), auth::deviceMac().c_str(),
                    WiFi.localIP().toString().c_str(), int(WiFi.status()), int(WiFi.RSSI()),
                    network.online() ? 1 : 0, session.ready ? 1 : 0);
      continue;
    }
    // wifi <ssid> <password>：直接写 NVS 并重启，绕过配网页
    if (cmd.startsWith("wifi ")) {
      String rest = cmd.substring(5);
      const int sp = rest.indexOf(' ');
      if (sp > 0) {
        String ssid = rest.substring(0, sp);
        String pass = rest.substring(sp + 1);
        ssid.trim();
        pass.trim();
        Preferences p;
        bool ok = p.begin("zlc-config", false);
        if (ok) {
          ok = p.putString("ssid", ssid) == ssid.length();
          ok = (p.putString("wifi", pass) == pass.length()) && ok;
          p.end();
        }
        Serial.printf("[cfg] ssid=%s passLen=%d saved=%d\n", ssid.c_str(), int(pass.length()), ok ? 1 : 0);
        delay(250);
        ESP.restart();
      } else {
        Serial.println("[cfg] usage: wifi <ssid> <password>");
      }
      continue;
    }
    if (cmd == "scan") {
      const int n = WiFi.scanNetworks();
      Serial.printf("[scan] %d aps\n", n);
      for (int i = 0; i < n; i++)
        Serial.printf("  ssid=%-24s rssi=%4d ch=%2d auth=%d\n", WiFi.SSID(i).c_str(),
                      int(WiFi.RSSI(i)), int(WiFi.channel(i)), int(WiFi.encryptionType(i)));
      continue;
    }
    if (cmd == "reboot") { Serial.println("[sys] reboot"); delay(80); ESP.restart(); }

    if (cmd == "onoff") { localStop(); Serial.println("[onenet] monitor off"); continue; }
    onenetcfg::handleLine(cmd, onenet, onenetActive);
  }
}
