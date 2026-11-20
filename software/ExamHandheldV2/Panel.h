#pragma once
#include <stdint.h>

namespace dev {

enum PanelCtrl { kST7735 = 0, kST7789 = 1, kILI9341 = 2 };

// 运行时配置的 SPI 面板驱动。引脚、控制器、分辨率全部由调用方给出，
// 便于同一份固件在自研板与验证板上运行。
class Panel {
 public:
  bool begin(int scl, int sda, int cs, int dc, int rst, int bl,
             int ctrl, int w, int h, int rotation);
  void fill(uint16_t color);
  void push(int x, int y, int w, int h, const uint16_t* data);
  void setRotation(int r);
  int width() const { return w_; }
  int height() const { return h_; }
  bool ready() const { return ready_; }
  void backlight(bool on);
  void sleep(bool off);

 private:
  void reset();
  void cmd(uint8_t c);
  void data(const uint8_t* d, unsigned n);
  void cmdData(uint8_t c, const uint8_t* d, unsigned n);
  void window(int x0, int y0, int x1, int y1);
  void applyMadctl();
  void initST7735();
  void initST7789();
  void initILI9341();

  int scl_ = -1, sda_ = -1, cs_ = -1, dc_ = -1, rst_ = -1, bl_ = -1;
  int ctrl_ = kST7735, w_ = 0, h_ = 0, rot_ = 0;
  int baseW_ = 0, baseH_ = 0;   // 面板原生宽高
  int xoff_ = 0, yoff_ = 0;     // 显存偏移
  bool ready_ = false;
};

}  // namespace dev
