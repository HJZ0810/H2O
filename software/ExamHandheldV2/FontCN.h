#pragma once
#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "Canvas.h"
#include "TextPainter.h"

namespace dev {

// 把 Adafruit_GFX 的 drawPixel 重定向到画布，让 U8g2 的中文字库直接写进我们的 16 位缓冲。
class CanvasGfx : public Adafruit_GFX {
 public:
  CanvasGfx() : Adafruit_GFX(320, 240) {}
  void attach(ui::Canvas* c) { c_ = c; }
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (c_) c_->set(x, y, ui::Color(color));
  }
 private:
  ui::Canvas* c_ = nullptr;
};

class FontCN : public ui::TextPainter {
 public:
  void attach(ui::Canvas* c) { gfx_.attach(c); u8_.begin(gfx_); u8_.setFontMode(1); }
  void setBold(bool b) { bold_ = b; }
  void useSize(int px) override;
  int lineHeight() override;
  int width(const char* utf8) override;
  void draw(ui::Canvas& c, int x, int yTop, const char* utf8, ui::Color col, uint8_t alpha = 255) override;

 private:
  CanvasGfx gfx_;
  U8G2_FOR_ADAFRUIT_GFX u8_;
  bool bold_ = false;
  int px_ = 0;
};

}  // namespace dev
