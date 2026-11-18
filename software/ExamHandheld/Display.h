#pragma once
#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <vector>
#include "Config.h"
class Display {
 public:
  void begin(const DeviceConfig& cfg);
  void show(const String& title,const String& body,const String& footer,const String& badge="",bool forceReset=false);
  void scroll(int lines);
  bool lastPage() const;
  void nextPage();
  int page() const {return offset_/Rows+1;}
 private:
  static constexpr int Rows=6;
  Adafruit_ST7735 tft_{&SPI,15,2,21};
  GFXcanvas16 canvas_{160,128};
  U8G2_FOR_ADAFRUIT_GFX font_;
  std::vector<String> lines_;
  String title_,body_,footer_,badge_;
  int offset_=0;
  bool dirty_=true;
  void wrap(const String& text);
  void draw();
};
