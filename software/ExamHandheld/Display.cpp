#include "Display.h"
#include "Board.h"
#include <algorithm>
static constexpr uint16_t Bg=0x08A3,HeaderBg=0x12AC,Fg=0xEF7D,Accent=0x3DBA,Muted=0x8C92;

static uint16_t blend565(uint16_t fg,uint16_t bg,uint8_t alpha) {
  const unsigned rest=15-alpha;
  const unsigned red=((fg>>11)*alpha+(bg>>11)*rest+7)/15;
  const unsigned green=(((fg>>5)&63)*alpha+((bg>>5)&63)*rest+7)/15;
  const unsigned blue=((fg&31)*alpha+(bg&31)*rest+7)/15;
  return (red<<11)|(green<<5)|blue;
}
void Display::begin(const DeviceConfig& cfg,glyph::Source* glyphs) {
  glyphs_=glyphs;
  SPI.begin(board::TFT_SCK,-1,board::TFT_MOSI,board::TFT_CS);
  const uint8_t tab[]={INITR_BLACKTAB,INITR_REDTAB,INITR_GREENTAB};
  tft_.initR(tab[cfg.panel<3?cfg.panel:0]);tft_.setSPISpeed(10000000);tft_.setRotation(cfg.rotation==3?3:1);
  tft_.fillScreen(Bg);font_.begin(canvas_);font_.setFont(u8g2_font_wqy12_t_gb2312);
  font_.setFontMode(1);font_.setForegroundColor(Fg);font_.setBackgroundColor(Bg);
}
int Display::localAdvance(uint32_t cp) {
  return cp<=0xFFFF ? u8g2_GetGlyphWidth(&font_.u8g2,cp) : 0;
}
int Display::textWidth(const String& text) {
  int width=0;
  for (size_t i=0;i<text.length();) {
    const auto character=glyph::decode(text.c_str()+i,text.length()-i);
    i+=character.length;
    if (glyph::variationSelector(character.codepoint)) continue;
    const int local=localAdvance(character.codepoint);
    width+=local>0 ? local : glyph::Side;
  }
  return width;
}

void Display::drawText(int x,int baseline,const String& text,uint16_t color,uint16_t background) {
  font_.setForegroundColor(color);
  for (size_t i=0;i<text.length() && x<160;) {
    const auto character=glyph::decode(text.c_str()+i,text.length()-i);
    i+=character.length;
    const auto cp=character.codepoint;
    if (glyph::variationSelector(cp)) continue;
    const int local=localAdvance(cp);
    if (local>0) {x+=font_.drawGlyph(x,baseline,cp);continue;}
    const bool remote=glyphs_ && glyph::serverHan(cp);
    const auto* bitmap=remote ? glyphs_->lookup(cp) : nullptr;
    const int top=baseline-10;
    if (bitmap) {
      for (int row=0;row<glyph::Side;++row) {
        for (int col=0;col<glyph::Side;++col) {
          const auto alpha=glyph::coverage(*bitmap,col,row);
          if (alpha) canvas_.drawFastHLine(x+col,top+row,1,blend565(color,background,alpha));
        }
      }
    } else {
      canvas_.drawFastHLine(x+1,top+1,10,color);
      canvas_.drawFastHLine(x+1,top+10,10,color);
      canvas_.drawFastVLine(x+1,top+1,10,color);
      canvas_.drawFastVLine(x+10,top+1,10,color);
      if (remote && std::find(missing_.begin(),missing_.end(),cp)==missing_.end()) missing_.push_back(cp);
    }
    x+=glyph::Side;
  }
}

void Display::wrap(const String& source) {
  lines_.clear();sourceOffsets_.clear();sourceOffsets_.push_back(0);String s=source;
  // GB2312 font does not contain all Unicode punctuation; preserve meaning with ASCII equivalents.
  s.replace("→","->");s.replace("♥","*");s.replace("℃","C");s.replace("—","-");s.replace("…","...");
  String line;
  int width=0;
  for(unsigned i=0;i<s.length();) {
    if(s[i]=='\r'){i++;continue;}
    if(s[i]=='\n'){lines_.push_back(line);sourceOffsets_.push_back(lines_.size());line="";width=0;i++;continue;}
    const auto character=glyph::decode(s.c_str()+i,s.length()-i);
    if (!glyph::variationSelector(character.codepoint)) {
      const auto local=localAdvance(character.codepoint);
      const int advance=local>0 ? local : glyph::Side;
      if (width+advance>152 && line.length()) {lines_.push_back(line);line="";width=0;}
      line=line+s.substring(i,i+character.length);
      width+=advance;
    }
    i+=character.length;
    if(lines_.size()>500)break;
  }
  lines_.push_back(line);
}
void Display::show(const String& title,const String& body,const String& footer,const String& badge,bool reset,int focusSourceLine) {
  if (glyphs_) {
    if (glyphs_->poll()) dirty_=true;
    if (!missing_.empty() && uint32_t(millis()-retriedAt_)>=500) {
      retriedAt_=millis();
      for (uint32_t cp : missing_) if (glyphs_->lookup(cp)) dirty_=true;
    }
  }
  if(reset){offset_=0;dirty_=true;}
  if(body_!=body){body_=body;wrap(body);dirty_=true;}
  if(title_!=title||footer_!=footer||badge_!=badge){title_=title;footer_=footer;badge_=badge;dirty_=true;}
  int max=lines_.empty()?0:int(lines_.size())-1;
  if(offset_>max)offset_=max;
  if(focusSourceLine>=0 && size_t(focusSourceLine)<sourceOffsets_.size()) {
    const int line=sourceOffsets_[focusSourceLine];
    const int end=size_t(focusSourceLine+1)<sourceOffsets_.size()?sourceOffsets_[focusSourceLine+1]:lines_.size();
    if(line<offset_ || end>offset_+Rows) {
      offset_=line;
      dirty_=true;
    }
  }
  if(dirty_)draw();
}
void Display::scroll(int n) {int next=offset_+n,max=lines_.empty()?0:int(lines_.size())-1;if(next<0)next=0;if(next>max)next=max;if(next!=offset_){offset_=next;dirty_=true;}}
bool Display::lastPage() const{return offset_+Rows>=int(lines_.size());}
void Display::nextPage(){scroll(Rows);}
void Display::draw() {
  if(!canvas_.getBuffer())return;
  missing_.clear();
  canvas_.fillScreen(Bg);canvas_.fillRect(0,0,160,20,HeaderBg);
  drawText(4,14,title_,Fg,HeaderBg);
  for(int i=0;i<Rows;i++) if(offset_+i<int(lines_.size()))drawText(4,33+i*13,lines_[offset_+i],Fg,Bg);
  canvas_.drawFastHLine(0,113,160,Accent);
  drawText(4,125,footer_,Muted,Bg);
  if(!badge_.isEmpty()) drawText(156-textWidth(badge_),14,badge_,Accent,HeaderBg);
  tft_.drawRGBBitmap(0,0,canvas_.getBuffer(),160,128);dirty_=false;
}
