#pragma once
#include <stdint.h>
#include <string.h>

namespace ui {

// RGB565，小端存放顺序由推送方向决定；内部统一按 16 位值运算。
struct Color {
  uint16_t v;
  constexpr Color() : v(0) {}
  constexpr explicit Color(uint16_t raw) : v(raw) {}
  static constexpr Color rgb(uint8_t r, uint8_t g, uint8_t b) {
    return Color(uint16_t(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)));
  }
  static constexpr Color hex(uint32_t x) {
    return rgb(uint8_t(x >> 16), uint8_t(x >> 8), uint8_t(x));
  }
  uint8_t r() const { return uint8_t(((v >> 11) & 0x1F) * 255 / 31); }
  uint8_t g() const { return uint8_t(((v >> 5) & 0x3F) * 255 / 63); }
  uint8_t b() const { return uint8_t((v & 0x1F) * 255 / 31); }
};

constexpr uint16_t toRaw(Color c) { return c.v; }

inline Color mix(Color a, Color b, uint8_t t) {  // t=0 → a, t=255 → b
  int ar = a.r(), ag = a.g(), ab = a.b();
  int br = b.r(), bg2 = b.g(), bb = b.b();
  return Color::rgb(uint8_t(ar + (br - ar) * t / 255),
                    uint8_t(ag + (bg2 - ag) * t / 255),
                    uint8_t(ab + (bb - ab) * t / 255));
}

// 16 位色画布。缓冲区由外部提供，便于设备端放在 PSRAM、主机端放在堆上。
class Canvas {
 public:
  Canvas() {}
  Canvas(uint16_t* buf, int w, int h) : buf_(buf), w_(w), h_(h) {}

  void attach(uint16_t* buf, int w, int h) { buf_ = buf; w_ = w; h_ = h; }
  uint16_t* data() { return buf_; }
  const uint16_t* data() const { return buf_; }
  int width() const { return w_; }
  int height() const { return h_; }
  bool valid() const { return buf_ && w_ > 0 && h_ > 0; }

  inline void set(int x, int y, Color c) {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
    buf_[y * w_ + x] = c.v;
  }
  inline Color get(int x, int y) const {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return Color();
    return Color(buf_[y * w_ + x]);
  }
  inline void blend(int x, int y, Color c, uint8_t a) {
    if (x < 0 || y < 0 || x >= w_ || y >= h_ || a == 0) return;
    if (a == 255) { buf_[y * w_ + x] = c.v; return; }
    buf_[y * w_ + x] = mix(Color(buf_[y * w_ + x]), c, a).v;
  }

  void clear(Color c) {
    if (!valid()) return;
    uint16_t v = c.v;
    for (int i = 0, n = w_ * h_; i < n; i++) buf_[i] = v;
  }
  void fillRect(int x, int y, int w, int h, Color c) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > w_ ? w_ : x + w, y1 = y + h > h_ ? h_ : y + h;
    for (int yy = y0; yy < y1; yy++) {
      uint16_t* p = buf_ + yy * w_;
      for (int xx = x0; xx < x1; xx++) p[xx] = c.v;
    }
  }
  void blendRect(int x, int y, int w, int h, Color c, uint8_t a) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > w_ ? w_ : x + w, y1 = y + h > h_ ? h_ : y + h;
    for (int yy = y0; yy < y1; yy++)
      for (int xx = x0; xx < x1; xx++) blend(xx, yy, c, a);
  }

 private:
  uint16_t* buf_ = nullptr;
  int w_ = 0, h_ = 0;
};

}  // namespace ui
