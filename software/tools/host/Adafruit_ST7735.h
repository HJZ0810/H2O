#pragma once

#include "Adafruit_GFX.h"

class SPIClass {
 public:
  void begin(int,int,int,int) {}
};
inline SPIClass SPI;
constexpr uint8_t INITR_BLACKTAB=0,INITR_REDTAB=1,INITR_GREENTAB=2;
extern std::vector<uint16_t> simulatedScreen;

class Adafruit_ST7735 {
 public:
  Adafruit_ST7735(SPIClass*,int,int,int) {}
  void initR(uint8_t) {}
  void setSPISpeed(uint32_t) {}
  void setRotation(uint8_t) {}
  void fillScreen(uint16_t) {}
  void drawRGBBitmap(int,int,const uint16_t* data,int w,int h) {
    simulatedScreen.assign(data,data+w*h);
  }
};
