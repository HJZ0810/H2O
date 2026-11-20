#pragma once
#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <vector>
#include "Config.h"
#include "Board.h"
#include "HanGlyph.h"
class Display {
 public:
  void begin(const DeviceConfig& cfg,glyph::Source* glyphs=nullptr);
  void show(const String& title,const String& body,const String& footer,const String& badge="",bool forceReset=false,
            int focusSourceLine=-1);
  void scroll(int lines);
  bool lastPage() const;
  void nextPage();
  int page() const {return offset_/Rows+1;}
 private:
  static constexpr int Rows=6;
  Adafruit_ST7735 tft_{&SPI,board::TFT_CS,board::TFT_DC,board::TFT_RST};
  GFXcanvas16 canvas_{160,128};
  U8G2_FOR_ADAFRUIT_GFX font_;
  std::vector<String> lines_;
  std::vector<int> sourceOffsets_;
  glyph::Source* glyphs_=nullptr;
  std::vector<uint32_t> missing_;
  String title_,body_,footer_,badge_;
  int offset_=0;
  bool dirty_=true;
  uint32_t retriedAt_=0;
  int localAdvance(uint32_t codepoint);
  int textWidth(const String& text);
  void drawText(int x,int baseline,const String& text,uint16_t color,uint16_t background);
  void wrap(const String& text);
  void draw();
};
