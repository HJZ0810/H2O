#pragma once
#include <mbedtls/base64.h>

// 无摄像头时的 UI 验收通道：串口注入按键 + 导出帧缓冲。
//
// 摄像头拍出来的屏太小、有摩尔纹，判不清字形细节；直接把 framebuf 逐像素导出来，
// 才能确定"这一行到底是字被裁了，还是相机糊了"。
//
// 命令：
//   key up|down|left|right|ok|back|test    短按注入一次
//   long up|down|...                       长按注入
//   page                                   打印当前 view / phase / menuIndex
//   shot                                   导出 160x128 RGB565，base64 分行
//
// 本文件必须在 render()/handleKey() 之后 include，因为它直接用这些静态函数。

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

static int parseKeyName(const String& n) {
  if (n == "test") return board::Test;
  if (n == "back") return board::Back;
  if (n == "up") return board::Up;
  if (n == "down") return board::Down;
  if (n == "left") return board::Left;
  if (n == "right") return board::Right;
  if (n == "ok") return board::Confirm;
  return -1;
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

static void pollUiDebug() {
  static String buf;
  while (Serial.available()) {
    const int ch = Serial.read();
    if (ch == 13) continue;
    if (ch != 10) {
      if (buf.length() < 300) buf += char(ch);
      continue;
    }
    buf.trim();
    const String cmd = buf;
    buf = "";
    if (cmd.isEmpty()) continue;

    if (cmd.startsWith("key ") || cmd.startsWith("long ")) {
      const bool isLong = cmd.startsWith("long ");
      const String name = cmd.substring(isLong ? 5 : 4);
      const int idx = parseKeyName(name);
      if (idx < 0) {
        Serial.println("[ui] unknown key");
      } else {
        handleKey(static_cast<board::Key>(idx), isLong ? Press::Long : Press::Short);
        render();
        Serial.print("[ui] key=");
        Serial.print(name);
        Serial.print(" long=");
        Serial.print(isLong ? 1 : 0);
        Serial.print(" view=");
        Serial.println(viewName(int(view)));
      }
      continue;
    }
    if (cmd == "page") {
      Serial.print("[ui] view=");
      Serial.print(viewName(int(view)));
      Serial.print(" phase=");
      Serial.print(int(session.phase));
      Serial.print(" menu=");
      Serial.print(menuIndex);
      Serial.print(" list=");
      Serial.println(view == View::Menu ? 6 : 0);
      continue;
    }
    if (cmd == "shot") { dumpShot(); continue; }

    onenetcfg::handleLine(cmd, onenet, onenetActive);
  }
}
