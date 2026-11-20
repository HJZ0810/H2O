#pragma once
#include "Canvas.h"

namespace ui {

struct Rect {
  int x = 0, y = 0, w = 0, h = 0;
  Rect() {}
  Rect(int x_, int y_, int w_, int h_) : x(x_), y(y_), w(w_), h(h_) {}
  int right() const { return x + w; }
  int bottom() const { return y + h; }
  int cx() const { return x + w / 2; }
  int cy() const { return y + h / 2; }
  bool contains(int px, int py) const { return px >= x && py >= y && px < right() && py < bottom(); }
};

namespace gfx {

void hLine(Canvas& c, int x, int y, int w, Color col);
void vLine(Canvas& c, int x, int y, int h, Color col);
void line(Canvas& c, int x0, int y0, int x1, int y1, Color col, int thick = 1);
void rect(Canvas& c, const Rect& r, Color col, int thick = 1);
void fillRect(Canvas& c, const Rect& r, Color col);
void fillRound(Canvas& c, const Rect& r, int radius, Color col);
void strokeRound(Canvas& c, const Rect& r, int radius, Color col, int thick = 1);
// 带抗锯齿的圆角卡片；fill 可为透明（alpha<255）
void card(Canvas& c, const Rect& r, int radius, Color fill, uint8_t fillAlpha, Color stroke, int thick);
void gradV(Canvas& c, const Rect& r, Color top, Color bottom);
void gradH(Canvas& c, const Rect& r, Color left, Color right);
void circle(Canvas& c, int cx, int cy, int radius, Color col, bool filled);
void ring(Canvas& c, int cx, int cy, int radius, int thick, Color col);
void tri(Canvas& c, int x0, int y0, int x1, int y1, int x2, int y2, Color col);
void glowLine(Canvas& c, int x, int y, int w, Color col);
// 1bpp / 8bpp 位图混合，用于字形与图标
void blit1bpp(Canvas& c, int x, int y, int w, int h, const uint8_t* bits, Color col, uint8_t alpha = 255);
void blitGray4(Canvas& c, int x, int y, int w, int h, const uint8_t* nibbles, Color col, uint8_t alpha = 255);
void blitGray8(Canvas& c, int x, int y, int w, int h, const uint8_t* gray, Color col, uint8_t alpha = 255);
// 图标（矢量绘制，尺寸自适应）
void iconSignal(Canvas& c, int x, int y, int size, Color col, int bars);
void iconWifi(Canvas& c, int x, int y, int size, Color col);
void iconBattery(Canvas& c, int x, int y, int w, int h, Color col, int percent);
void iconCheck(Canvas& c, int x, int y, int size, Color col);
void iconCross(Canvas& c, int x, int y, int size, Color col);
void iconChevron(Canvas& c, int x, int y, int size, Color col, bool right);
void iconGear(Canvas& c, int cx, int cy, int radius, Color col);
void iconThermo(Canvas& c, int x, int y, int h, Color col);
void iconDrop(Canvas& c, int cx, int cy, int size, Color col);

}  // namespace gfx
}  // namespace ui
