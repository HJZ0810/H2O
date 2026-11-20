#include "Display.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>

std::vector<uint16_t> simulatedScreen;

class TestGlyphs : public glyph::Source {
 public:
  explicit TestGlyphs(const std::filesystem::path& path) {
    if (!path.empty()) {
      atlas_.open(path,std::ios::binary);
      assert(atlas_.good());
    }
  }
  const glyph::Bitmap* lookup(uint32_t codepoint) override {
    requested.insert(codepoint);
    if (!ready) return nullptr;
    if (atlas_.is_open()) {
      atlas_.seekg(codepoint*glyph::Bytes);
      atlas_.read(reinterpret_cast<char*>(record_.data()),record_.size());
      assert(atlas_.good());
    } else {
      record_.fill(0);
      record_[0]=0xF0;
      record_[10]=0x0F;
      record_[11]=0x70;
    }
    return &record_;
  }
  bool poll() override {const bool now=changed;changed=false;return now;}
  std::set<uint32_t> requested;
  bool ready=false,changed=false;
 private:
  std::ifstream atlas_;
  glyph::Bitmap record_{};
};

void saveFrame(const std::filesystem::path& path) {
  std::ofstream file(path,std::ios::binary);
  file<<"P6\n160 128\n255\n";
  for (uint16_t color : simulatedScreen) {
    file.put((color>>11)*255/31);
    file.put(((color>>5)&63)*255/63);
    file.put((color&31)*255/31);
  }
  assert(file.good());
}

int main(int argc,char** argv) {
  const auto rare=glyph::decode("𠮷",4);
  assert(rare.codepoint==0x20BB7 && rare.length==4);
  assert(glyph::decode("龘",3).codepoint==0x9F98);
  assert(glyph::decode("\xC0\xAF",2).codepoint==0xFFFD);
  assert(glyph::decode("\xED\xA0\x80",3).codepoint==0xFFFD);
  assert(glyph::decode("\xF0\xA0",2).length==1);
  assert(glyph::decode("\xF4\x90\x80\x80",4).codepoint==0xFFFD);
  assert(glyph::serverHan(0x33479) && !glyph::serverHan(0x3347A));

  TestGlyphs synthetic(std::filesystem::path{});
  Display pixelTest;
  pixelTest.begin(DeviceConfig{},&synthetic);
  pixelTest.show("","𠮷","","",true);
  synthetic.ready=true;
  synthetic.changed=true;
  pixelTest.show("","𠮷","");
  const auto color=[](int x,int y){return simulatedScreen.at(y*160+x);};
  assert(color(4,23)==0xEF7D && color(13,24)==0xEF7D && color(15,23)!=0xEF7D);
  assert(color(14,24)!=0xEF7D && color(14,24)!=0x08A3);

  TestGlyphs glyphs(argc==3 ? std::filesystem::path(argv[2]) : std::filesystem::path());
  Display tft;
  tft.begin(DeviceConfig{},&glyphs);
  const char* title="第 2 关 多选 1/3";
  const char* question="工程组在走廊布置无线传感节点，终端采集温湿度和气体传感器的模拟输出。"
                       "部署时需要确认数据来源、量程和设备连接状态。下面哪些做法正确？";
  const String page=String(question)+"\n\n  [ ] A 记录温度和湿度\n  [ ] B 校验模拟输入量程\n"
                                    "  [ ] C 交给设备自行判分\n  [ ] D 忽略数据有效标志";
  tft.show(title,page,"F1选 F2消 OK交","在线",true);
  assert(simulatedScreen.size()==160*128 && tft.page()==1 && !tft.lastPage());
  const auto firstFrame=simulatedScreen;
  assert(std::count(firstFrame.begin(),firstFrame.end(),0xEF7D)>180);
  const auto output=argc>=2 ? std::filesystem::path(argv[1]) : std::filesystem::path();
  if (!output.empty()) saveFrame(output/"tft-question.ppm");
  const String selected=String(question)+"\n\n  [x] A 记录温度和湿度\n> [x] B 校验模拟输入量程\n"
                                        "  [ ] C 交给设备自行判分\n  [ ] D 忽略数据有效标志";
  tft.show(title,selected,"F1选 F2消 OK交","在线",false,3);
  assert(tft.page()>1 && simulatedScreen!=firstFrame);
  assert(std::count(simulatedScreen.begin(),simulatedScreen.end(),0xEF7D)>140);
  if (!output.empty()) saveFrame(output/"tft-options.ppm");

  if (argc==3) {
    glyphs.requested.clear();
    const char* samples="常用汉字\n小龙虾与肉夹馍\n\n生僻汉字\n𠮷 龘 𰻞 𲎰";
    tft.show("汉字显示校验",samples,"UTF-8",{},true);
    const auto offline=simulatedScreen;
    assert(glyphs.requested.count(0x20BB7)==1 && glyphs.requested.count(0x30EDE)==1);
    assert(glyphs.requested.count(0x867E)==0);
    glyphs.ready=true;
    glyphs.changed=true;
    tft.show("汉字显示校验",samples,"UTF-8");
    assert(simulatedScreen!=offline);
    if (!output.empty()) saveFrame(output/"tft-han.ppm");
  }
  std::cout<<"ST7735 framebuffer, UTF-8, asymmetric glyph orientation, wrapping and focus: PASS\n";
}
