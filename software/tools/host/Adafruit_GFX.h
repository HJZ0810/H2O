#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

class Print {
 public:
  virtual size_t write(uint8_t)=0;
  virtual size_t write(const uint8_t*,size_t)=0;
};

class Adafruit_GFX {
 public:
  virtual void drawFastHLine(int16_t,int16_t,int16_t,uint16_t)=0;
  virtual void drawFastVLine(int16_t,int16_t,int16_t,uint16_t)=0;
};

class GFXcanvas16 : public Adafruit_GFX {
 public:
  GFXcanvas16(int width,int height) : width_(width),height_(height),pixels_(width*height) {}
  uint16_t* getBuffer() {return pixels_.data();}
  void fillScreen(uint16_t color) {std::fill(pixels_.begin(),pixels_.end(),color);}
  void fillRect(int x,int y,int w,int h,uint16_t color) {
    for (int row=0;row<h;++row) drawFastHLine(x,y+row,w,color);
  }
  void drawFastHLine(int16_t x,int16_t y,int16_t count,uint16_t color) override {
    for (int i=0;i<count;++i) plot(x+i,y,color);
  }
  void drawFastVLine(int16_t x,int16_t y,int16_t count,uint16_t color) override {
    for (int i=0;i<count;++i) plot(x,y+i,color);
  }
 private:
  int width_,height_;
  std::vector<uint16_t> pixels_;
  void plot(int x,int y,uint16_t color) {
    if (x>=0 && x<width_ && y>=0 && y<height_) pixels_[y*width_+x]=color;
  }
};
