#include "Panel.h"
#include <Arduino.h>
#include <SPI.h>

namespace dev {

static SPIClass s_bus(FSPI);
static uint16_t s_line[512];

void Panel::cmd(uint8_t c) {
  digitalWrite(dc_, LOW);
  digitalWrite(cs_, LOW);
  s_bus.transfer(c);
  digitalWrite(cs_, HIGH);
}
void Panel::data(const uint8_t* d, unsigned n) {
  digitalWrite(dc_, HIGH);
  digitalWrite(cs_, LOW);
  for (unsigned i = 0; i < n; i++) s_bus.transfer(d[i]);
  digitalWrite(cs_, HIGH);
}
void Panel::cmdData(uint8_t c, const uint8_t* d, unsigned n) {
  digitalWrite(dc_, LOW);
  digitalWrite(cs_, LOW);
  s_bus.transfer(c);
  digitalWrite(dc_, HIGH);
  for (unsigned i = 0; i < n; i++) s_bus.transfer(d[i]);
  digitalWrite(cs_, HIGH);
}
void Panel::window(int x0, int y0, int x1, int y1) {
  x0 += xoff_; x1 += xoff_; y0 += yoff_; y1 += yoff_;
  uint8_t a[4] = {uint8_t(x0 >> 8), uint8_t(x0), uint8_t(x1 >> 8), uint8_t(x1)};
  cmdData(0x2A, a, 4);
  uint8_t b[4] = {uint8_t(y0 >> 8), uint8_t(y0), uint8_t(y1 >> 8), uint8_t(y1)};
  cmdData(0x2B, b, 4);
  cmd(0x2C);
}
void Panel::reset() {
  if (rst_ < 0) { cmd(0x01); delay(120); return; }
  digitalWrite(rst_, HIGH); delay(10);
  digitalWrite(rst_, LOW);  delay(30);
  digitalWrite(rst_, HIGH); delay(200);
}
void Panel::backlight(bool on) { if (bl_ >= 0) digitalWrite(bl_, on ? HIGH : LOW); }

void Panel::applyMadctl() {
  // 0/2 竖屏，1/3 横屏（交换行列）
  uint8_t m = 0x00;
  switch (rot_ & 3) {
    case 0: m = 0x00; break;
    case 1: m = 0x60; break;   // MX | MV
    case 2: m = 0xC0; break;   // MX | MY
    case 3: m = 0xA0; break;   // MY | MV
  }
  cmdData(0x36, &m, 1);
}

void Panel::initST7735() {
  reset();
  cmd(0x01); delay(150);
  cmd(0x11); delay(200);
  { uint8_t d[] = {0x01, 0x2C, 0x2D}; cmdData(0xB1, d, 3); }
  { uint8_t d[] = {0x01, 0x2C, 0x2D}; cmdData(0xB2, d, 3); }
  { uint8_t d[] = {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D}; cmdData(0xB3, d, 6); }
  { uint8_t d[] = {0x07}; cmdData(0xB4, d, 1); }
  { uint8_t d[] = {0xA2, 0x02, 0x84}; cmdData(0xC0, d, 3); }
  { uint8_t d[] = {0xC5}; cmdData(0xC1, d, 1); }
  { uint8_t d[] = {0x0A, 0x00}; cmdData(0xC2, d, 2); }
  { uint8_t d[] = {0x8A, 0x2A}; cmdData(0xC3, d, 2); }
  { uint8_t d[] = {0x8A, 0xEE}; cmdData(0xC4, d, 2); }
  { uint8_t d[] = {0x0E}; cmdData(0xC5, d, 1); }
  applyMadctl();
  { uint8_t d[] = {0x05}; cmdData(0x3A, d, 1); }   // 16bit
  { uint8_t d[] = {0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D,
                   0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10};
    cmdData(0xE0, d, 16); }
  { uint8_t d[] = {0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
                   0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10};
    cmdData(0xE1, d, 16); }
  cmd(0x13); delay(10);
  cmd(0x29); delay(50);
}

void Panel::initST7789() {
  reset();
  cmd(0x01); delay(150);
  cmd(0x11); delay(200);
  applyMadctl();
  { uint8_t d[] = {0x55}; cmdData(0x3A, d, 1); }
  { uint8_t d[] = {0x0C, 0x0C, 0x00, 0x33, 0x33}; cmdData(0xB2, d, 5); }
  { uint8_t d[] = {0x35}; cmdData(0xB7, d, 1); }
  { uint8_t d[] = {0x19}; cmdData(0xBB, d, 1); }
  { uint8_t d[] = {0x2C}; cmdData(0xC0, d, 1); }
  { uint8_t d[] = {0x01}; cmdData(0xC2, d, 1); }
  { uint8_t d[] = {0x12}; cmdData(0xC3, d, 1); }
  { uint8_t d[] = {0x20}; cmdData(0xC4, d, 1); }
  { uint8_t d[] = {0x0F}; cmdData(0xC6, d, 1); }
  { uint8_t d[] = {0xA4, 0xA1}; cmdData(0xD0, d, 2); }
  cmd(0x21);
  cmd(0x29); delay(50);
}

void Panel::initILI9341() {
  reset();
  cmd(0x01); delay(150);
  cmd(0x11); delay(200);
  applyMadctl();
  { uint8_t d[] = {0x55}; cmdData(0x3A, d, 1); }
  { uint8_t d[] = {0xC3, 0x25}; cmdData(0x26, d, 1); }  // 伽马
  { uint8_t d[] = {0x08, 0x82, 0x1B}; cmdData(0xC0, d, 3); }
  { uint8_t d[] = {0x00}; cmdData(0xC1, d, 1); }
  { uint8_t d[] = {0x0F, 0x0F}; cmdData(0xC5, d, 2); }
  cmd(0x29); delay(50);
}

bool Panel::begin(int scl, int sda, int cs, int dc, int rst, int bl,
                  int ctrl, int w, int h, int rotation) {
  scl_ = scl; sda_ = sda; cs_ = cs; dc_ = dc; rst_ = rst; bl_ = bl;
  ctrl_ = ctrl; rot_ = rotation;
  baseW_ = w; baseH_ = h;

  pinMode(cs_, OUTPUT); digitalWrite(cs_, HIGH);
  pinMode(dc_, OUTPUT);
  if (rst_ >= 0) { pinMode(rst_, OUTPUT); digitalWrite(rst_, HIGH); }
  if (bl_ >= 0) { pinMode(bl_, OUTPUT); digitalWrite(bl_, HIGH); }

  s_bus.begin(scl_, -1, sda_, -1);
  s_bus.setFrequency(20000000);

  if (rot_ & 1) { w_ = baseH_; h_ = baseW_; }
  else { w_ = baseW_; h_ = baseH_; }

  // 1.8" 128x160 常见显存偏移；命中不同批次时由调用方覆盖
  xoff_ = 0; yoff_ = 0;

  switch (ctrl_) {
    case kST7735: initST7735(); break;
    case kST7789: initST7789(); break;
    default: initILI9341(); break;
  }
  ready_ = true;
  return true;
}

void Panel::setRotation(int r) {
  rot_ = r & 3;
  if (rot_ & 1) { w_ = baseH_; h_ = baseW_; }
  else { w_ = baseW_; h_ = baseH_; }
  applyMadctl();
}

void Panel::fill(uint16_t color) {
  if (!ready_) return;
  window(0, 0, w_ - 1, h_ - 1);
  uint16_t sw = (color >> 8) | (color << 8);
  for (int i = 0; i < 512; i++) s_line[i] = sw;
  digitalWrite(dc_, HIGH); digitalWrite(cs_, LOW);
  int total = w_ * h_;
  while (total > 0) {
    int n = total > 512 ? 512 : total;
    s_bus.writeBytes((const uint8_t*)s_line, n * 2);
    total -= n;
  }
  digitalWrite(cs_, HIGH);
}

void Panel::push(int x, int y, int w, int h, const uint16_t* data) {
  if (!ready_ || !data || w <= 0 || h <= 0) return;
  window(x, y, x + w - 1, y + h - 1);
  digitalWrite(dc_, HIGH); digitalWrite(cs_, LOW);
  for (int r = 0; r < h; r++) {
    const uint16_t* src = data + r * w;
    int i = 0;
    while (i < w) {
      int n = (w - i) > 512 ? 512 : (w - i);
      for (int k = 0; k < n; k++) s_line[k] = (src[i + k] >> 8) | (src[i + k] << 8);
      s_bus.writeBytes((const uint8_t*)s_line, n * 2);
      i += n;
    }
  }
  digitalWrite(cs_, HIGH);
}

void Panel::sleep(bool off) {
  if (!ready_) return;
  if (off) { cmd(0x28); cmd(0x10); delay(20); }
  else { cmd(0x11); delay(120); cmd(0x29); delay(20); }
}

}  // namespace dev
