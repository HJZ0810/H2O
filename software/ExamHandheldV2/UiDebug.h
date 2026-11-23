#pragma once
#include <mbedtls/base64.h>

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

    if (cmd == "onoff") { localStop(); Serial.println("[onenet] monitor off"); continue; }
    onenetcfg::handleLine(cmd, onenet, onenetActive);
  }
}
