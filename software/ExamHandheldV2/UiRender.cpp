#include <stdio.h>
#include "UiRender.h"

namespace ui {

void Painter::cardBg(const Rect& r, bool glow) {
  if (glow) {
    c_.blendRect(r.x - 1, r.y - 1, r.w + 2, r.h + 2, t_.accent, 26);
    c_.blendRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4, t_.accent, 12);
  }
  gfx::fillRound(c_, r, t_.radius, t_.panel);
  gfx::strokeRound(c_, r, t_.radius, glow ? t_.accent : t_.stroke, 1);
}

void Painter::cornerBrackets(const Rect& r, Color col) {
  int len = t_.compact ? 6 : 9;
  int th = 2;
  c_.fillRect(r.x, r.y, len, th, col);
  c_.fillRect(r.x, r.y, th, len, col);
  c_.fillRect(r.right() - len, r.y, len, th, col);
  c_.fillRect(r.right() - th, r.y, th, len, col);
  c_.fillRect(r.x, r.bottom() - th, len, th, col);
  c_.fillRect(r.x, r.bottom() - len, th, len, col);
  c_.fillRect(r.right() - len, r.bottom() - th, len, th, col);
  c_.fillRect(r.right() - th, r.bottom() - len, th, len, col);
}

void Painter::background() {
  gfx::gradV(c_, Rect(0, 0, t_.W, t_.H), Color::hex(0x071018), t_.bg);
  // 细网格
  int step = t_.compact ? 12 : 18;
  for (int x = 0; x < t_.W; x += step) c_.blendRect(x, 0, 1, t_.H, Color::hex(0x1B3446), 26);
  for (int y = 0; y < t_.H; y += step) c_.blendRect(0, y, t_.W, 1, Color::hex(0x1B3446), 20);
  // 顶部光带
  gfx::gradH(c_, Rect(0, 0, t_.W, 1), t_.bg, t_.accent);
  c_.blendRect(0, 0, t_.W / 2, 1, t_.accent, 220);
  // 右下角装饰弧
  int rr = t_.W / 2;
  for (int i = 0; i < rr; i += 2) {
    Color cc = mix(t_.bg, t_.accent2, uint8_t(10 + i * 40 / rr));
    c_.blend(t_.W - 1 - i, t_.H - 1 - (rr - i) / 3, cc, 60);
  }
}

void Painter::statusBar(const Status& s) {
  int h = t_.statusH;
  c_.fillRect(0, 0, t_.W, h, Color::hex(0x060C12));
  c_.blendRect(0, h - 1, t_.W, 1, t_.stroke, 200);

  int x = t_.pad;
  int iconS = h - 8;
  if (iconS < 8) iconS = 8;
  int bars = s.rssi >= -55 ? 4 : s.rssi >= -65 ? 3 : s.rssi >= -75 ? 2 : s.rssi != 0 ? 1 : 0;
  gfx::iconSignal(c_, x, (h - iconS) / 2 + 1, iconS, s.rssi ? t_.accent : t_.dimmer, bars);
  x += iconS + 4;

  f_.useSize(t_.smallPx);
  if (s.group && *s.group) {
    char buf[24];
    snprintf(buf, sizeof(buf), "组 %s", s.group);
    f_.draw(c_, x, (h - f_.lineHeight()) / 2, buf, t_.text);
    x += f_.width(buf) + 6;
  }
  // 在线指示
  int dot = t_.compact ? 4 : 5;
  Color oc = s.online ? t_.ok : t_.danger;
  gfx::circle(c_, x + dot / 2, h / 2, dot / 2, oc, true);
  c_.blendRect(x, h / 2 - dot / 2 - 1, dot + 2, dot + 2, oc, 40);
  f_.draw(c_, x + dot + 4, (h - f_.lineHeight()) / 2, s.online ? "在线" : "离线",
          s.online ? t_.ok : t_.danger);

  // 右侧：分数 + 电池
  int rx = t_.W - t_.pad;
  int bw = t_.compact ? 14 : 18, bh = t_.compact ? 7 : 9;
  rx -= bw + 2;
  gfx::iconBattery(c_, rx, (h - bh) / 2, bw, bh, s.battery >= 0 ? t_.dim : t_.dimmer, s.battery);
  rx -= 6;
  if (s.scoreKnown) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%d分", s.score);
    f_.useSize(t_.smallPx);
    int w = f_.width(buf);
    rx -= w;
    f_.draw(c_, rx, (h - f_.lineHeight()) / 2, buf, t_.accent);
  }
}

void Painter::pageTitle(const char* title, const char* badge, Color badgeCol) {
  int y = t_.statusH;
  int h = t_.titleH;
  // 左侧竖条
  int barH = h - (t_.compact ? 8 : 10);
  gfx::gradV(c_, Rect(t_.pad, y + (h - barH) / 2, 3, barH), t_.accent, t_.accent2);
  f_.useSize(t_.titlePx);
  int th = f_.lineHeight();
  f_.draw(c_, t_.pad + 9, y + (h - th) / 2 - 1, title, t_.text);

  if (badge && *badge) {
    int pw = pillWidth(badge) + 0;
    pill(t_.W - t_.pad - (pw - 12), y + (h - (t_.compact ? 16 : 20)) / 2, badge, badgeCol, Color::hex(0x0A1A22));
  }
  c_.blendRect(t_.pad, y + h - 1, t_.W - 2 * t_.pad, 1, t_.stroke, 160);
}

int Painter::pillWidth(const char* text) {
  f_.useSize(t_.compact ? 9 : 10);
  int w = f_.width(text);
  int pad = t_.compact ? 5 : 7;
  return w + pad * 2 + 2;
}

void Painter::pill(int x, int y, const char* text, Color fg, Color bg) {
  f_.useSize(t_.compact ? 9 : 10);
  int pad = t_.compact ? 5 : 7;
  int w = f_.width(text);
  int h = t_.compact ? 15 : 19;
  Rect r(x, y, w + pad * 2, h);
  gfx::fillRound(c_, r, h / 2, bg);
  gfx::strokeRound(c_, r, h / 2, fg, 1);
  f_.draw(c_, x + pad, y + (h - f_.lineHeight()) / 2, text, fg);
}

void Painter::footerBar(const char* left, const char* right) {
  int y = t_.H - t_.footerH;
  c_.fillRect(0, y, t_.W, 1, t_.stroke);
  c_.blendRect(0, y + 1, t_.W, t_.footerH - 1, Color::hex(0x060C12), 220);
  f_.useSize(t_.smallPx);
  int th = f_.lineHeight();
  int ty = y + (t_.footerH - th) / 2;
  // 键帽
  int x = t_.pad;
  struct KeyCap { const char* k; const char* label; };
  // 依据文本中的 "A:动作 B:动作" 简化：整体绘制左侧提示
  if (left && *left) f_.draw(c_, x, ty, left, t_.dim);
  if (right && *right) {
    int w = f_.width(right);
    f_.draw(c_, t_.W - t_.pad - w, ty, right, t_.dimmer);
  }
}

void Painter::menu(const MenuEntry* items, int count, int selected) {
  int top = t_.contentTop() + t_.gap;
  int bottom = t_.contentBottom() - t_.gap;
  int avail = bottom - top;
  if (count <= 0) return;

  // 小屏只露出能放下的行数，按选中项滚动，右侧给滚动条
  int rowH = t_.rowH;
  int cap = (avail - 2 * t_.gap) / 3;
  if (cap > 0 && rowH > cap) rowH = cap;
  int vis = (avail + t_.gap) / (rowH + t_.gap);
  if (vis < 1) vis = 1;
  if (vis > count) vis = count;
  int start = selected - vis + 1;
  if (start < 0) start = 0;
  if (start > count - vis) start = count - vis;
  if (start < 0) start = 0;

  int totalH = rowH * vis + t_.gap * (vis - 1);
  int y = top + (avail - totalH) / 2;

  for (int k = 0; k < vis; k++) {
    int i = start + k;
    Rect r(t_.pad, y, t_.W - 2 * t_.pad, rowH);
    bool sel = (i == selected);
    if (sel) {
      c_.blendRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4, t_.accent, 16);
      gfx::fillRound(c_, r, t_.radius, t_.panelHi);
      gfx::strokeRound(c_, r, t_.radius, t_.accent, 1);
      gfx::gradV(c_, Rect(r.x, r.y + t_.radius / 2, 3, r.h - t_.radius), t_.accent, t_.accent2);
    } else {
      gfx::fillRound(c_, r, t_.radius, t_.panel);
      gfx::strokeRound(c_, r, t_.radius, t_.stroke, 1);
    }

    // 图标底座
    int is = rowH - (t_.compact ? 12 : 16);
    int icx = r.x + (t_.compact ? 12 : 16);
    int icy = r.cy();
    Color ic = sel ? t_.accent : t_.dim;
    switch (items[i].kind) {
      case 0:  // 考试：播放三角
        gfx::tri(c_, icx - is / 3, icy - is / 2 + 2, icx - is / 3, icy + is / 2 - 2, icx + is / 2, icy, ic);
        break;
      case 1:  // 环境：温度计 + 水滴
        gfx::iconThermo(c_, icx - is / 6, icy - is / 2 + 2, is - 4, ic);
        gfx::iconDrop(c_, icx + is / 3, icy + 1, is / 2, ic);
        break;
      case 2:  // 信息：芯片
        gfx::strokeRound(c_, Rect(icx - is / 2, icy - is / 2 + 2, is, is - 4), 2, ic, 1);
        for (int k = -1; k <= 1; k++) {
          c_.fillRect(icx + k * is / 4, icy - is / 2, 1, 2, ic);
          c_.fillRect(icx + k * is / 4, icy + is / 2 - 2, 1, 2, ic);
        }
        break;
      case 3:  // 设置：齿轮
        gfx::iconGear(c_, icx, icy, is / 3, ic);
        break;
      default:  // 执行器：风扇
        gfx::circle(c_, icx, icy, is / 2 - 2, ic, false);
        gfx::line(c_, icx - is / 3, icy, icx + is / 3, icy, ic, 1);
        gfx::line(c_, icx, icy - is / 3, icx, icy + is / 3, ic, 1);
        break;
    }

    int tx = r.x + (t_.compact ? 26 : 34);
    f_.useSize(t_.bodyPx);
    int lh = f_.lineHeight();
    if (items[i].sub && *items[i].sub) {
      f_.draw(c_, tx, r.y + (r.h - lh * 2) / 2 + 1, items[i].label, sel ? t_.text : t_.text);
      f_.useSize(t_.smallPx);
      f_.draw(c_, tx, r.y + (r.h - lh) / 2 + lh - 2, items[i].sub, sel ? t_.accent : t_.dimmer);
    } else {
      f_.draw(c_, tx, r.cy() - lh / 2, items[i].label, sel ? t_.text : t_.text);
    }
    gfx::iconChevron(c_, r.right() - (t_.compact ? 13 : 18), r.cy() - (t_.compact ? 4 : 6),
                     t_.compact ? 9 : 12, sel ? t_.accent : t_.dimmer, true);
    y += rowH + t_.gap;
  }

  if (vis < count) {
    int sx = t_.W - 2;
    int th = bottom - top;
    int bh = th * vis / count;
    if (bh < 10) bh = 10;
    int by = top + (th - bh) * start / (count - vis);
    c_.blendRect(sx, top, 1, th, t_.stroke, 90);
    c_.fillRect(sx, by, 1, bh, t_.accent);
  }
}

int Painter::questionHeight(const QuestionView& q) {
  int w = t_.W - 2 * t_.pad;
  f_.useSize(t_.bodyPx);
  int lh = f_.lineHeight();
  int starts[24], lens[24];
  int n = f_.wrap(q.text, w - 2 * t_.pad, starts, lens, 24);
  int total = n * lh + 2 * t_.pad - 4 + t_.gap;
  int optW = w - (t_.compact ? 38 : 46);
  for (int i = 0; i < q.optionCount; i++) {
    int s2[4], l2[4];
    int m = f_.wrap(q.options[i].text, optW, s2, l2, 4);
    int oh = 2 * (t_.compact ? 3 : 5) + m * lh;
    if (oh < (t_.compact ? 24 : 32)) oh = (t_.compact ? 24 : 32);
    total += oh + (i + 1 < q.optionCount ? t_.gap - 1 : 0);
  }
  return total;
}

void Painter::question(const QuestionView& q, int scroll) {
  int x = t_.pad;
  int w = t_.W - 2 * t_.pad;
  int top = t_.statusH + t_.gap;
  int viewH = questionViewport();
  int totalH = questionHeight(q);
  int maxScroll = totalH > viewH ? totalH - viewH : 0;
  if (scroll < 0) scroll = 0;
  if (scroll > maxScroll) scroll = maxScroll;
  int y = top - scroll;

  // 题干卡片
  f_.useSize(t_.bodyPx);
  int lh = f_.lineHeight();
  int starts[24], lens[24];
  int n = f_.wrap(q.text, w - 2 * t_.pad, starts, lens, 24);
  int cardH = n * lh + 2 * t_.pad - 4;
  Rect body(x, y, w, cardH);
  cardBg(body);
  cornerBrackets(body, t_.accent);
  for (int i = 0; i < n; i++) {
    char line[256];
    int len = lens[i] < (int)sizeof(line) - 1 ? lens[i] : (int)sizeof(line) - 1;
    memcpy(line, q.text + starts[i], len);
    line[len] = 0;
    f_.draw(c_, x + t_.pad, y + t_.pad - 4 + i * lh, line, t_.text);
  }
  y += cardH + t_.gap;

  // 选项：高度按剩余空间均分，长文本自动折行
  int count = q.optionCount;
  if (count <= 0) return;
  int optGap = t_.gap - 1;
  int remain = viewH - cardH - t_.gap - optGap * (count - 1);
  int optH = remain / count;
  int maxOpt = t_.compact ? 34 : 52;
  int minOpt = t_.compact ? 24 : 34;
  if (optH > maxOpt) optH = maxOpt;
  if (optH < minOpt) optH = minOpt;

  for (int i = 0; i < count; i++) {
    Rect r(x, y, w, optH);
    bool sel = q.options[i].selected;
    bool removed = q.options[i].removed;
    bool cur = (i == q.cursor);
    Color base = removed ? Color::hex(0x0A1016) : sel ? Color::hex(0x0B2A32) : t_.panel;
    gfx::fillRound(c_, r, t_.radius, base);
    Color st = cur ? t_.accent : sel ? Color::hex(0x1E6E7A) : t_.stroke;
    gfx::strokeRound(c_, r, t_.radius, st, cur ? 2 : 1);
    if (cur) c_.blendRect(r.x - 1, r.y - 1, r.w + 2, r.h + 2, t_.accent, 22);

    int cr = t_.compact ? 8 : 10;
    int ccx = r.x + (t_.compact ? 4 : 6) + cr, ccy = r.cy();
    if (removed) {
      gfx::circle(c_, ccx, ccy, cr, t_.dimmer, false);
      gfx::iconCross(c_, ccx - cr + 3, ccy - cr + 3, cr * 2 - 6, t_.dimmer);
    } else if (sel) {
      gfx::circle(c_, ccx, ccy, cr, t_.accent, true);
      f_.useSize(t_.smallPx);
      int kw = f_.width(q.options[i].key);
      f_.draw(c_, ccx - kw / 2, ccy - f_.lineHeight() / 2, q.options[i].key, t_.onAccent);
    } else {
      gfx::circle(c_, ccx, ccy, cr, cur ? t_.accent : t_.dim, false);
      f_.useSize(t_.smallPx);
      int kw = f_.width(q.options[i].key);
      f_.draw(c_, ccx - kw / 2, ccy - f_.lineHeight() / 2, q.options[i].key, cur ? t_.accent : t_.dim);
    }

    int tx = ccx + cr + (t_.compact ? 6 : 9);
    f_.useSize(t_.bodyPx);
    int tlh = f_.lineHeight();
    int s2[4], l2[4];
    int m = f_.wrap(q.options[i].text, r.right() - tx - 5, s2, l2, 4);
    int th = m * tlh;
    int ty = r.y + (r.h - th) / 2;
    for (int k = 0; k < m; k++) {
      char line[128];
      int len = l2[k] < 127 ? l2[k] : 127;
      memcpy(line, q.options[i].text + s2[k], len); line[len] = 0;
      f_.draw(c_, tx, ty + k * tlh, line, removed ? t_.dimmer : t_.text);
    }
    y += optH + optGap;
  }

  // 滚动条
  if (maxScroll > 0) {
    int sx = t_.W - 3;
    c_.blendRect(sx, top, 2, viewH, t_.stroke, 120);
    int thumbH = viewH * viewH / totalH;
    if (thumbH < 16) thumbH = 16;
    int ty = top + (viewH - thumbH) * scroll / maxScroll;
    gfx::fillRound(c_, Rect(sx, ty, 2, thumbH), 1, t_.accent);
  }
}

void Painter::story(int level, const char* text, int seq, int total) {
  int x = t_.pad, w = t_.W - 2 * t_.pad;
  int y = t_.contentTop() + t_.gap;
  Rect r(x, y, w, t_.contentBottom() - t_.gap - y);
  cardBg(r);
  cornerBrackets(r, t_.accent2);
  f_.useSize(t_.bodyPx);
  int lh = f_.lineHeight();
  int starts[40], lens[40];
  int n = f_.wrap(text, w - 2 * t_.pad, starts, lens, 40);
  int maxN = (r.h - 2 * t_.pad) / lh;
  if (n > maxN) n = maxN;
  for (int i = 0; i < n; i++) {
    char line[256];
    int len = lens[i] < 255 ? lens[i] : 255;
    memcpy(line, text + starts[i], len); line[len] = 0;
    f_.draw(c_, x + t_.pad, y + t_.pad + i * lh, line, t_.text);
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "%d / %d", seq, total);
  f_.useSize(t_.smallPx);
  f_.draw(c_, r.right() - t_.pad - f_.width(buf), r.bottom() - t_.pad - f_.lineHeight(), buf, t_.dimmer);
}

void Painter::judge(bool correct, const char* answer, int gain, int hp, int score, const char* text) {
  int x = t_.pad, w = t_.W - 2 * t_.pad;
  int y = t_.contentTop() + t_.gap;
  Color ac = correct ? t_.ok : t_.danger;
  Rect head(x, y, w, t_.compact ? 34 : 46);
  c_.blendRect(head.x - 2, head.y - 2, head.w + 4, head.h + 4, ac, 20);
  gfx::fillRound(c_, head, t_.radius, Color::hex(0x0B1A16));
  gfx::strokeRound(c_, head, t_.radius, ac, 1);
  int icx = x + t_.pad + 8;
  if (correct) gfx::iconCheck(c_, icx - 6, head.cy() - 6, 14, ac);
  else gfx::iconCross(c_, icx - 6, head.cy() - 6, 14, ac);
  f_.useSize(t_.bodyPx);
  f_.draw(c_, icx + 14, head.cy() - f_.lineHeight() / 2, correct ? "回答正确" : "判题解析", ac);

  char buf[48];
  snprintf(buf, sizeof(buf), "+%d", gain);
  f_.useSize(t_.titlePx);
  f_.draw(c_, head.right() - t_.pad - f_.width(buf), head.cy() - f_.lineHeight() / 2, buf, ac);

  y += head.h + t_.gap;
  f_.useSize(t_.smallPx);
  int lh = f_.lineHeight();
  snprintf(buf, sizeof(buf), "正确答案 %s", answer);
  f_.draw(c_, x + 2, y, buf, t_.dim);
  y += lh + 2;
  snprintf(buf, sizeof(buf), "生命 %d   累计 %d 分", hp, score);
  f_.draw(c_, x + 2, y, buf, t_.dim);
  y += lh + t_.gap;

  Rect body(x, y, w, t_.contentBottom() - t_.gap - y);
  cardBg(body);
  f_.useSize(t_.bodyPx);
  int blh = f_.lineHeight();
  int starts[32], lens[32];
  int n = f_.wrap(text, w - 2 * t_.pad, starts, lens, 32);
  int maxN = (body.h - 2 * t_.pad) / blh;
  if (n > maxN) n = maxN;
  for (int i = 0; i < n; i++) {
    char line[256];
    int len = lens[i] < 255 ? lens[i] : 255;
    memcpy(line, text + starts[i], len); line[len] = 0;
    f_.draw(c_, x + t_.pad, y + t_.pad + i * blh, line, t_.dim);
  }
}

void Painter::kv(int x, int y, int w, const char* k, const char* v, Color vc) {
  f_.useSize(t_.smallPx);
  f_.draw(c_, x, y, k, t_.dimmer);
  f_.useSize(t_.bodyPx);
  int vw = f_.width(v);
  f_.draw(c_, x + w - vw, y - 1, v, vc);
}

void Painter::bar(int x, int y, int w, int h, int percent, Color col) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  gfx::fillRound(c_, Rect(x, y, w, h), h / 2, Color::hex(0x0A141C));
  if (percent > 0) {
    int fw = w * percent / 100;
    if (fw < h) fw = h;
    gfx::fillRound(c_, Rect(x, y, fw, h), h / 2, col);
    c_.blendRect(x, y - 1, fw, h + 2, col, 30);
  }
}

void Painter::sensorPage(bool dhtValid, float temp, float humidity,
                         const Metric* mq, int mqCount, bool simulating) {
  int x = t_.pad, w = t_.W - 2 * t_.pad;
  int y = t_.contentTop() + t_.gap;
  int half = (w - t_.gap) / 2;

  // 温湿度两块大数字卡
  const char* labels[2] = {"温度", "湿度"};
  char vals[2][16];
  if (dhtValid) {
    snprintf(vals[0], sizeof(vals[0]), "%.1f", temp);
    snprintf(vals[1], sizeof(vals[1]), "%.1f", humidity);
  } else {
    snprintf(vals[0], sizeof(vals[0]), "--");
    snprintf(vals[1], sizeof(vals[1]), "--");
  }
  const char* units[2] = {"°C", "%RH"};
  Color cols[2] = {Color::hex(0xFF8A5B), t_.accent};
  for (int i = 0; i < 2; i++) {
    Rect r(x + i * (half + t_.gap), y, half, t_.compact ? 58 : 78);
    cardBg(r);
    f_.useSize(t_.smallPx);
    f_.draw(c_, r.x + t_.pad, r.y + t_.pad, labels[i], t_.dim);
    if (i == 0) gfx::iconThermo(c_, r.right() - t_.pad - 4, r.y + t_.pad, 12, cols[i]);
    else gfx::iconDrop(c_, r.right() - t_.pad - 4, r.y + t_.pad + 4, 9, cols[i]);
    f_.useSize(t_.compact ? 20 : 30);
    f_.draw(c_, r.x + t_.pad, r.bottom() - t_.pad - f_.lineHeight() + 2, vals[i], cols[i]);
    f_.useSize(t_.smallPx);
    f_.draw(c_, r.right() - t_.pad - f_.width(units[i]), r.bottom() - t_.pad - f_.lineHeight(), units[i], t_.dim);
  }
  y += (t_.compact ? 58 : 78) + t_.gap;

  // 气体两路
  for (int i = 0; i < mqCount; i++) {
    Rect r(x, y, w, t_.compact ? 40 : 54);
    cardBg(r);
    f_.useSize(t_.bodyPx);
    f_.draw(c_, r.x + t_.pad, r.y + t_.pad - 2, mq[i].label, t_.text);
    f_.useSize(t_.smallPx);
    int lh2 = f_.lineHeight();
    f_.draw(c_, r.x + t_.pad, r.bottom() - t_.pad - lh2, mq[i].value, t_.dim);
    // 右侧大数值
    f_.useSize(t_.bodyPx);
    int vw = f_.width(mq[i].unit);
    f_.draw(c_, r.right() - t_.pad - vw, r.cy() - f_.lineHeight() / 2 + 1, mq[i].unit, mq[i].color);
    y += r.h + t_.gap;
  }
  if (simulating) {
    f_.useSize(t_.smallPx);
    f_.draw(c_, x + 2, t_.contentBottom() - f_.lineHeight() - 2, "MQ 通道为模拟值（验证板无该传感器）", t_.warn);
  }
}

void Painter::infoPage(const char* const* keys, const char* const* values, int count) {
  int x = t_.pad, w = t_.W - 2 * t_.pad;
  int y = t_.contentTop() + t_.gap;
  int rowH = t_.compact ? 22 : 28;
  int maxRows = (t_.contentBottom() - t_.gap - y) / rowH;
  Rect r(x, y, w, (count < maxRows ? count : maxRows) * rowH + t_.pad);
  cardBg(r);
  int yy = y + t_.pad / 2 + 2;
  for (int i = 0; i < count && i < maxRows; i++) {
    kv(x + t_.pad, yy, w - 2 * t_.pad, keys[i], values[i], t_.text);
    if (i + 1 < count && i + 1 < maxRows) c_.blendRect(x + t_.pad, yy + rowH - 5, w - 2 * t_.pad, 1, t_.stroke, 90);
    yy += rowH;
  }
}

void Painter::setupPage(const char* ap, const char* pass, const char* url, const char* mac) {
  int x = t_.pad, w = t_.W - 2 * t_.pad;
  int y = t_.contentTop() + t_.gap;
  Rect r(x, y, w, t_.contentBottom() - t_.gap - y);
  cardBg(r);
  cornerBrackets(r, t_.accent);
  f_.useSize(t_.smallPx);
  int lh = f_.lineHeight();
  int yy = y + t_.pad;
  f_.draw(c_, x + t_.pad, yy, "手机连接热点", t_.dim); yy += lh + 4;
  f_.useSize(t_.bodyPx);
  f_.draw(c_, x + t_.pad, yy, ap, t_.accent); yy += f_.lineHeight() + 6;
  f_.useSize(t_.smallPx);
  f_.draw(c_, x + t_.pad, yy, "密码", t_.dim); yy += lh + 2;
  f_.useSize(t_.bodyPx);
  f_.draw(c_, x + t_.pad, yy, pass, t_.text); yy += f_.lineHeight() + 6;
  f_.useSize(t_.smallPx);
  f_.draw(c_, x + t_.pad, yy, "浏览器打开", t_.dim); yy += lh + 2;
  f_.useSize(t_.bodyPx);
  f_.draw(c_, x + t_.pad, yy, url, t_.accent); yy += f_.lineHeight() + 6;
  f_.useSize(t_.smallPx);
  f_.draw(c_, x + t_.pad, yy, mac, t_.dimmer);
}

void Painter::fanPage(bool linked, bool channelB, int pctA, int pctB, int pulseUs, bool servoActive) {
  int x = t_.pad, w = t_.W - 2 * t_.pad;
  int y = t_.contentTop() + t_.gap;

  // 连接状态条
  Rect st(x, y, w, t_.compact ? 22 : 28);
  gfx::fillRound(c_, st, t_.radius / 2, linked ? Color::hex(0x0A2018) : Color::hex(0x241016));
  gfx::strokeRound(c_, st, t_.radius / 2, linked ? t_.ok : t_.danger, 1);
  f_.useSize(t_.smallPx);
  f_.draw(c_, st.x + t_.pad, st.cy() - f_.lineHeight() / 2,
          linked ? "执行板已连接" : "执行板未连接", linked ? t_.ok : t_.danger);
  y += st.h + t_.gap;

  // A/B 两路
  for (int i = 0; i < 2; i++) {
    int pct = i == 0 ? pctA : pctB;
    bool act = (i == 0) ? !channelB : channelB;
    Rect r(x, y, w, t_.compact ? 50 : 68);
    cardBg(r, act);
    Color col = i == 0 ? t_.accent : t_.accent2;
    f_.useSize(t_.bodyPx);
    char label[16];
    snprintf(label, sizeof(label), "%s 路", i == 0 ? "A" : "B");
    f_.draw(c_, r.x + t_.pad, r.y + t_.pad - 2, label, act ? col : t_.dim);
    char val[16];
    snprintf(val, sizeof(val), "%d%%", pct);
    f_.useSize(t_.titlePx);
    f_.draw(c_, r.right() - t_.pad - f_.width(val), r.y + t_.pad - 4, val, act ? t_.text : t_.dim);
    bar(r.x + t_.pad, r.bottom() - t_.pad - (t_.compact ? 7 : 9), r.w - 2 * t_.pad, t_.compact ? 7 : 9, pct, col);
    y += r.h + t_.gap;
  }

  // 舵机
  Rect r(x, y, w, t_.compact ? 40 : 52);
  cardBg(r, servoActive);
  f_.useSize(t_.bodyPx);
  f_.draw(c_, r.x + t_.pad, r.y + t_.pad - 2, "舵机", t_.text);
  char buf[32];
  if (servoActive) snprintf(buf, sizeof(buf), "%d us", pulseUs);
  else snprintf(buf, sizeof(buf), "待命");
  f_.useSize(t_.titlePx);
  f_.draw(c_, r.right() - t_.pad - f_.width(buf), r.y + t_.pad - 4, buf, servoActive ? t_.accent : t_.dimmer);
}

void Painter::finishedPage(int score, int rank, const char* certificate) {
  int x = t_.pad, w = t_.W - 2 * t_.pad;
  int y = t_.contentTop() + t_.gap;
  Rect r(x, y, w, t_.contentBottom() - t_.gap - y);
  cardBg(r);
  cornerBrackets(r, t_.ok);
  f_.useSize(t_.smallPx);
  f_.draw(c_, r.cx() - f_.width("已通关") / 2, y + t_.pad, "已通关", t_.ok);
  char buf[24];
  snprintf(buf, sizeof(buf), "%d", score);
  f_.useSize(t_.compact ? 30 : 44);
  f_.draw(c_, r.cx() - f_.width(buf) / 2, y + (t_.compact ? 26 : 40), buf, t_.accent);
  f_.useSize(t_.smallPx);
  f_.draw(c_, r.cx() - f_.width("总分") / 2, y + (t_.compact ? 52 : 76), "总分", t_.dim);
  snprintf(buf, sizeof(buf), "排名 %d", rank);
  f_.useSize(t_.bodyPx);
  f_.draw(c_, r.cx() - f_.width(buf) / 2, y + (t_.compact ? 70 : 100), buf, t_.text);
  if (certificate && *certificate) {
    f_.useSize(t_.smallPx);
    f_.draw(c_, x + t_.pad, r.bottom() - t_.pad - f_.lineHeight(), certificate, t_.dimmer);
  }
}

void Painter::recoveryPage(const char* notice, bool online) {
  int x = t_.pad, w = t_.W - 2 * t_.pad;
  int y = t_.contentTop() + t_.gap;
  Rect r(x, y, w, t_.contentBottom() - t_.gap - y);
  cardBg(r);
  Color col = online ? t_.warn : t_.danger;
  gfx::ring(c_, r.cx(), y + (t_.compact ? 40 : 56), t_.compact ? 12 : 17, 3, col);
  f_.useSize(t_.smallPx);
  f_.draw(c_, r.cx() - f_.width(online ? "同步中" : "离线") / 2,
          y + (t_.compact ? 58 : 80), online ? "同步中" : "离线", col);
  f_.useSize(t_.bodyPx);
  int lh = f_.lineHeight();
  int starts[16], lens[16];
  int n = f_.wrap(notice, w - 2 * t_.pad, starts, lens, 16);
  for (int i = 0; i < n; i++) {
    char line[256];
    int len = lens[i] < 255 ? lens[i] : 255;
    memcpy(line, notice + starts[i], len); line[len] = 0;
    f_.draw(c_, r.cx() - f_.width(line) / 2, y + (t_.compact ? 80 : 112) + i * lh, line, t_.dim);
  }
}

void Painter::toast(const char* text) {
  f_.useSize(t_.smallPx);
  int w = f_.width(text) + t_.pad * 3;
  int h = f_.lineHeight() + t_.pad;
  Rect r((t_.W - w) / 2, t_.contentBottom() - h - t_.gap, w, h);
  c_.blendRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4, Color::hex(0x000000), 90);
  gfx::fillRound(c_, r, h / 2, Color::hex(0x12232E));
  gfx::strokeRound(c_, r, h / 2, t_.accent, 1);
  f_.draw(c_, r.cx() - f_.width(text) / 2, r.y + (h - f_.lineHeight()) / 2, text, t_.text);
}

}  // namespace ui
