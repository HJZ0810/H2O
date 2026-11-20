#include "FontCN.h"

namespace dev {

void FontCN::useSize(int px) {
  if (px == px_ && px_ != 0) return;
  px_ = px;
  // 只有 12 与 16 两档中文点阵；其余尺寸就近取档，避免字库缺失。
  u8_.setFont(px <= 14 ? u8g2_font_wqy12_t_gb2312 : u8g2_font_wqy16_t_gb2312);
}

int FontCN::lineHeight() {
  if (px_ == 0) useSize(12);
  return px_ <= 14 ? 14 : 18;
}

int FontCN::width(const char* utf8) {
  if (!utf8 || !*utf8) return 0;
  if (px_ == 0) useSize(12);
  return u8_.getUTF8Width(utf8);
}

void FontCN::draw(ui::Canvas& c, int x, int yTop, const char* utf8, ui::Color col, uint8_t alpha) {
  if (!utf8 || !*utf8) return;
  if (px_ == 0) useSize(12);
  int ascent = u8_.getFontAscent();
  u8_.setForegroundColor(col.v);
  u8_.drawUTF8(x, yTop + ascent, utf8);
  if (bold_) {
    // 伪粗体：右侧与下方各补一次，补偿点阵字在深色底上的单薄感
    u8_.drawUTF8(x + 1, yTop + ascent, utf8);
    u8_.drawUTF8(x, yTop + ascent + 1, utf8);
  }
  (void)c; (void)alpha;
}

}  // namespace dev
