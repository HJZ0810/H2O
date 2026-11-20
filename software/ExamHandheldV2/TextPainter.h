#pragma once
#include "Canvas.h"

namespace ui {

// 文本绘制的平台接口：设备端接 U8g2 点阵字库，主机端接 TrueType 光栅化。
// 坐标系约定：draw 的 y 为文本行的顶部，内部自行换算基线。
class TextPainter {
 public:
  virtual ~TextPainter() {}
  virtual void useSize(int px) = 0;
  virtual int lineHeight() = 0;
  virtual int width(const char* utf8) = 0;
  virtual void draw(Canvas& c, int x, int yTop, const char* utf8, Color col, uint8_t alpha = 255) = 0;

  // 按像素宽度折行。返回行数；starts/lens 由调用方提供，容量 maxLines。
  int wrap(const char* utf8, int maxWidth, int* starts, int* lens, int maxLines) {
    int n = 0;
    int len = (int)strlen(utf8);
    int i = 0;
    char buf[512];
    while (i < len && n < maxLines) {
      int lineStart = i, lastBreak = -1, lastBreakNext = -1;
      int j = i;
      int best = i;  // 当前行能容纳到的位置
      while (j < len) {
        int step = utf8Len(utf8[j]);
        if (j + step > len) break;
        if (j + step - lineStart > (int)sizeof(buf) - 1) break;
        memcpy(buf, utf8 + lineStart, j + step - lineStart);
        buf[j + step - lineStart] = 0;
        // 跳过行首空格
        if (width(buf) > maxWidth) break;
        best = j + step;
        if (utf8[j] == '\n') { lastBreak = j; lastBreakNext = j + 1; break; }
        if (utf8[j] == ' ' || utf8[j] == ',' || utf8[j] == '.' ||
            utf8[j] == ';' || utf8[j] == ':' || utf8[j] == '/') {
          lastBreak = j; lastBreakNext = j + 1;
        }
        j += step;
      }
      int end = best;
      int next = best;
      if (best >= len) { end = len; next = len; }
      else if (lastBreak > lineStart) { end = lastBreak; next = lastBreakNext; }
      if (end <= lineStart) { end = lineStart + utf8Len(utf8[lineStart]); next = end; }
      starts[n] = lineStart;
      lens[n] = end - lineStart;
      n++;
      i = next;
      while (i < len && utf8[i] == ' ') i++;
      if (utf8[i - 1] == '\n' && n < maxLines) { continue; }
    }
    return n;
  }

  static int utf8Len(char c) {
    unsigned char u = (unsigned char)c;
    if (u < 0x80) return 1;
    if ((u & 0xE0) == 0xC0) return 2;
    if ((u & 0xF0) == 0xE0) return 3;
    if ((u & 0xF8) == 0xF0) return 4;
    return 1;
  }
};

}  // namespace ui
