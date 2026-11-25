#pragma once
#include "Canvas.h"
#include "Gfx.h"
#include "Theme.h"
#include "TextPainter.h"

namespace ui {

struct Status {
  const char* group = "";
  const char* mac = "";
  int battery = -1;       // -1 未知
  int rssi = 0;           // 0 表示无信号
  bool online = false;
  int score = 0;
  bool scoreKnown = false;
  const char* clock = nullptr;
};

struct MenuEntry {
  const char* label;
  const char* sub;
  int kind;  // 0 考试 1 环境 2 信息 3 设置 4 执行器
};

struct OptionView {
  const char* key;
  const char* text;
  bool selected = false;
  bool removed = false;
};

struct QuestionView {
  int level = 1;
  const char* typeName = "";
  const char* text = "";
  const OptionView* options = nullptr;
  int optionCount = 0;
  int cursor = 0;
  int seq = 0;
  int total = 0;
  const char* hint = nullptr;
};

struct Metric {
  const char* label;
  const char* value;
  const char* unit;
  Color color;
};

class Painter {
 public:
  Painter(Canvas& c, Theme& t, TextPainter& f) : c_(c), t_(t), f_(f) {}

  void background();
  void statusBar(const Status& s);
  void pageTitle(const char* title, const char* badge = nullptr, Color badgeCol = Color::hex(0x00E5FF));
  void footerBar(const char* left, const char* right);
  void menu(const MenuEntry* items, int count, int selected);
  void question(const QuestionView& q, int scroll = 0);
  // 题干与选项的总高度；主程序据此计算最大滚动量
  int questionHeight(const QuestionView& q);
  int questionViewport() const { return t_.H - t_.statusH - t_.footerH - 2 * t_.gap; }
  void story(int level, const char* text, int seq, int total);
  void judge(bool correct, const char* answer, int gain, int hp, int score, const char* text);
  // scroll：内容整体上移的像素数，由上下键调节，滚出内容区的部分被裁掉
  void sensorPage(bool dhtValid, float temp, float humidity,
                  const Metric* mq, int mqCount, bool simulating, int scroll = 0);
  int sensorHeight(int mqCount, bool simulating) const;
  void infoPage(const char* const* keys, const char* const* values, int count, int scroll = 0);
  int infoHeight(int count) const;
  void setupPage(const char* ap, const char* pass, const char* url, const char* mac);
  void fanPage(bool linked, bool channelB, int pctA, int pctB, int pulseUs, bool servoActive);
  void finishedPage(int score, int rank, const char* certificate);
  void recoveryPage(const char* notice, bool online);

  // 通用小组件
  void pill(int x, int y, const char* text, Color fg, Color bg);
  int pillWidth(const char* text);
  void kv(int x, int y, int w, const char* k, const char* v, Color vc);
  void bar(int x, int y, int w, int h, int percent, Color col);
  void toast(const char* text);

 private:
  Canvas& c_;
  Theme& t_;
  TextPainter& f_;
  void cardBg(const Rect& r, bool glow = false);
  void cornerBrackets(const Rect& r, Color col);
};

}  // namespace ui
