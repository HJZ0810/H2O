#pragma once
#include "Canvas.h"

namespace ui {

// 全部度量按短边缩放，保证 128x160、240x320、320x480 三种面板都能成立。
struct Theme {
  int W = 240, H = 320;
  bool wide = false;      // 横屏
  bool compact = false;   // 小屏（短边 < 200）

  int pad = 10;           // 页面外边距
  int gap = 8;            // 卡片间距
  int radius = 10;        // 卡片圆角
  int statusH = 22;       // 顶部状态栏
  int titleH = 30;        // 页面标题
  int footerH = 24;       // 底部按键提示
  int rowH = 40;          // 菜单行高
  int lineStep = 14;      // 单行滚动步长
  int bodyPx = 16;        // 正文字号
  int smallPx = 11;
  int titlePx = 18;

  Color bg, bgDeep, panel, panelHi, stroke, strokeHi;
  Color accent, accent2, text, dim, dimmer, ok, warn, danger, onAccent;

  void layout(int w, int h) {
    W = w; H = h;
    int s = w < h ? w : h;
    compact = s < 200;
    if (s <= 140) {
      // 1.8 寸 128x160：横屏 160x128，只保留一行信息层级
      pad = 4; gap = 3; radius = 4;
      statusH = 14; titleH = 16; footerH = 13; rowH = 26;
      bodyPx = 12; smallPx = 12; titlePx = 12; lineStep = 14;
    } else if (s < 200) {
      pad = 5; gap = 4; radius = 5;
      statusH = 15; titleH = 20; footerH = 15; rowH = 28;
      bodyPx = 12; smallPx = 11; titlePx = 14; lineStep = 14;
    } else {
      float k = float(s) / 240.0f;
      pad = int(10 * k); if (pad < 5) pad = 5;
      gap = int(8 * k);  if (gap < 4) gap = 4;
      radius = int(10 * k); if (radius < 5) radius = 5;
      statusH = int(22 * k); if (statusH < 15) statusH = 15;
      titleH = int(30 * k); if (titleH < 20) titleH = 20;
      footerH = int(24 * k); if (footerH < 17) footerH = 17;
      rowH = int(42 * k);
      bodyPx = 16; smallPx = 11; titlePx = 18; lineStep = 20;
    }

    bg      = Color::hex(0x05080D);
    bgDeep  = Color::hex(0x020407);
    panel   = Color::hex(0x0D1620);
    panelHi = Color::hex(0x142230);
    stroke  = Color::hex(0x1D3242);
    strokeHi= Color::hex(0x2B4A60);
    accent  = Color::hex(0x00E5FF);
    accent2 = Color::hex(0x7C5CFF);
    text    = Color::hex(0xE6F4FF);
    dim     = Color::hex(0x7E97AC);
    dimmer  = Color::hex(0x41586B);
    ok      = Color::hex(0x00E676);
    warn    = Color::hex(0xFFB300);
    danger  = Color::hex(0xFF3D57);
    onAccent= Color::hex(0x03161C);
  }

  int contentTop() const { return statusH + titleH; }
  int contentBottom() const { return H - footerH; }
  int contentH() const { return contentBottom() - contentTop(); }
};

}  // namespace ui
