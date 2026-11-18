#include "Display.h"
#include "Board.h"
static constexpr uint16_t Bg=0x08A3,Fg=0xEF7D,Accent=0x3DBA,Muted=0x8C92;
void Display::begin(const DeviceConfig& cfg) {
  SPI.begin(board::TFT_SCK,-1,board::TFT_MOSI,board::TFT_CS);
  const uint8_t tab[]={INITR_BLACKTAB,INITR_REDTAB,INITR_GREENTAB};
  tft_.initR(tab[cfg.panel<3?cfg.panel:0]);tft_.setSPISpeed(10000000);tft_.setRotation(cfg.rotation==3?3:1);
  tft_.fillScreen(Bg);font_.begin(canvas_);font_.setFont(u8g2_font_wqy12_t_gb2312);
  font_.setFontMode(1);font_.setForegroundColor(Fg);font_.setBackgroundColor(Bg);
}
void Display::wrap(const String& source) {
  lines_.clear();String s=source;
  // GB2312 font does not contain all Unicode punctuation; preserve meaning with ASCII equivalents.
  s.replace("→","->");s.replace("♥","*");s.replace("℃","C");s.replace("—","-");s.replace("…","...");
  String line;
  for(unsigned i=0;i<s.length();) {
    if(s[i]=='\r'){i++;continue;}
    if(s[i]=='\n'){lines_.push_back(line);line="";i++;continue;}
    unsigned char c=s[i];unsigned len=c<0x80?1:((c&0xe0)==0xc0?2:((c&0xf0)==0xe0?3:4));
    if(i+len>s.length())len=1;
    String glyph=s.substring(i,i+len),next=line+glyph;
    if(font_.getUTF8Width(next.c_str())>152 && line.length()){lines_.push_back(line);line=glyph;}
    else line=next;
    i+=len;
    if(lines_.size()>500)break;
  }
  lines_.push_back(line);
}
void Display::show(const String& title,const String& body,const String& footer,const String& badge,bool reset) {
  if(reset){offset_=0;dirty_=true;}
  if(body_!=body){body_=body;wrap(body);dirty_=true;}
  if(title_!=title||footer_!=footer||badge_!=badge){title_=title;footer_=footer;badge_=badge;dirty_=true;}
  int max=lines_.empty()?0:int(lines_.size())-1;
  if(offset_>max)offset_=max;
  if(dirty_)draw();
}
void Display::scroll(int n) {int next=offset_+n,max=lines_.empty()?0:int(lines_.size())-1;if(next<0)next=0;if(next>max)next=max;if(next!=offset_){offset_=next;dirty_=true;}}
bool Display::lastPage() const{return offset_+Rows>=int(lines_.size());}
void Display::nextPage(){scroll(Rows);}
void Display::draw() {
  if(!canvas_.getBuffer())return;
  canvas_.fillScreen(Bg);canvas_.fillRect(0,0,160,20,0x12AC);
  font_.setForegroundColor(Fg);font_.drawUTF8(4,14,title_.c_str());
  for(int i=0;i<Rows;i++) if(offset_+i<int(lines_.size()))font_.drawUTF8(4,33+i*13,lines_[offset_+i].c_str());
  canvas_.drawFastHLine(0,113,160,Accent);
  font_.setForegroundColor(Muted);font_.drawUTF8(4,125,footer_.c_str());
  if(!badge_.isEmpty()){font_.setForegroundColor(Accent);int w=font_.getUTF8Width(badge_.c_str());font_.drawUTF8(156-w,14,badge_.c_str());}
  tft_.drawRGBBitmap(0,0,canvas_.getBuffer(),160,128);dirty_=false;
}
