#include "Gfx.h"
#include <math.h>

namespace ui {
namespace gfx {

static inline int iabs(int v) { return v < 0 ? -v : v; }

void hLine(Canvas& c, int x, int y, int w, Color col) { c.fillRect(x, y, w, 1, col); }
void vLine(Canvas& c, int x, int y, int h, Color col) { c.fillRect(x, y, 1, h, col); }

void line(Canvas& c, int x0, int y0, int x1, int y1, Color col, int thick) {
  int dx = iabs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -iabs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  int half = thick / 2;
  for (;;) {
    if (thick <= 1) c.set(x0, y0, col);
    else c.fillRect(x0 - half, y0 - half, thick, thick, col);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void rect(Canvas& c, const Rect& r, Color col, int thick) {
  c.fillRect(r.x, r.y, r.w, thick, col);
  c.fillRect(r.x, r.bottom() - thick, r.w, thick, col);
  c.fillRect(r.x, r.y, thick, r.h, col);
  c.fillRect(r.right() - thick, r.y, thick, r.h, col);
}
void fillRect(Canvas& c, const Rect& r, Color col) { c.fillRect(r.x, r.y, r.w, r.h, col); }

// 圆角矩形：中间三条矩形 + 四角逐像素按距离判定，边缘 1px 做抗锯齿。
void fillRound(Canvas& c, const Rect& r, int radius, Color col) {
  if (r.w <= 0 || r.h <= 0) return;
  int rad = radius;
  if (rad > r.w / 2) rad = r.w / 2;
  if (rad > r.h / 2) rad = r.h / 2;
  if (rad <= 0) { c.fillRect(r.x, r.y, r.w, r.h, col); return; }
  c.fillRect(r.x + rad, r.y, r.w - 2 * rad, r.h, col);
  c.fillRect(r.x, r.y + rad, rad, r.h - 2 * rad, col);
  c.fillRect(r.right() - rad, r.y + rad, rad, r.h - 2 * rad, col);
  const float rr = float(rad) * rad;
  for (int j = 0; j < rad; j++) {
    for (int i = 0; i < rad; i++) {
      float dx = float(rad - i) - 0.5f, dy = float(rad - j) - 0.5f;
      float d = dx * dx + dy * dy;
      if (d <= rr - rad) {
        c.set(r.x + i, r.y + j, col);
        c.set(r.right() - 1 - i, r.y + j, col);
        c.set(r.x + i, r.bottom() - 1 - j, col);
        c.set(r.right() - 1 - i, r.bottom() - 1 - j, col);
      } else if (d <= rr + rad) {
        float cov = (rr + rad - d) / (2.0f * rad);
        if (cov > 1) cov = 1;
        uint8_t a = uint8_t(cov * 255);
        c.blend(r.x + i, r.y + j, col, a);
        c.blend(r.right() - 1 - i, r.y + j, col, a);
        c.blend(r.x + i, r.bottom() - 1 - j, col, a);
        c.blend(r.right() - 1 - i, r.bottom() - 1 - j, col, a);
      }
    }
  }
}

void strokeRound(Canvas& c, const Rect& r, int radius, Color col, int thick) {
  // 通过"外框填充减内框填充"得到描边，避免逐像素扫描线误差
  int rad = radius;
  if (rad > r.w / 2) rad = r.w / 2;
  if (rad > r.h / 2) rad = r.h / 2;
  // 四条边
  c.fillRect(r.x + rad, r.y, r.w - 2 * rad, thick, col);
  c.fillRect(r.x + rad, r.bottom() - thick, r.w - 2 * rad, thick, col);
  c.fillRect(r.x, r.y + rad, thick, r.h - 2 * rad, col);
  c.fillRect(r.right() - thick, r.y + rad, thick, r.h - 2 * rad, col);
  // 四角圆弧
  const float rin = float(rad - thick);
  const float rout = float(rad);
  for (int j = 0; j < rad; j++) {
    for (int i = 0; i < rad; i++) {
      float dx = float(rad - i) - 0.5f, dy = float(rad - j) - 0.5f;
      float d = sqrtf(dx * dx + dy * dy);
      if (d <= rout && d >= rin - 0.5f) {
        uint8_t a = 255;
        if (d > rout - 0.5f) a = uint8_t(255 * (rout - d));
        if (d < rin + 0.5f && d >= rin - 0.5f) a = uint8_t(255 * (d - (rin - 0.5f)));
        c.blend(r.x + i, r.y + j, col, a);
        c.blend(r.right() - 1 - i, r.y + j, col, a);
        c.blend(r.x + i, r.bottom() - 1 - j, col, a);
        c.blend(r.right() - 1 - i, r.bottom() - 1 - j, col, a);
      }
    }
  }
}

void card(Canvas& c, const Rect& r, int radius, Color fill, uint8_t fillAlpha, Color stroke, int thick) {
  if (fillAlpha >= 255) fillRound(c, r, radius, fill);
  else if (fillAlpha > 0) {
    // 半透明：先画不透明底再叠加，保证圆角边缘干净
    for (int y = r.y; y < r.bottom(); y++)
      for (int x = r.x; x < r.right(); x++) c.blend(x, y, fill, fillAlpha);
    // 用背景色遮掉圆角外的四个小三角
    // 简化处理：圆角处按距离混合背景
    int rad = radius;
    if (rad > r.w / 2) rad = r.w / 2;
    if (rad > r.h / 2) rad = r.h / 2;
    const float rr = float(rad) * rad;
    for (int j = 0; j < rad; j++) {
      for (int i = 0; i < rad; i++) {
        float dx = float(rad - i) - 0.5f, dy = float(rad - j) - 0.5f;
        if (dx * dx + dy * dy > rr + rad) {
          c.set(r.x + i, r.y + j, c.get(r.x + i, r.y + j));
        }
      }
    }
  }
  if (thick > 0) strokeRound(c, r, radius, stroke, thick);
}

void gradV(Canvas& c, const Rect& r, Color top, Color bottom) {
  for (int y = 0; y < r.h; y++) {
    Color col = mix(top, bottom, uint8_t(r.h <= 1 ? 0 : y * 255 / (r.h - 1)));
    c.fillRect(r.x, r.y + y, r.w, 1, col);
  }
}
void gradH(Canvas& c, const Rect& r, Color left, Color right) {
  for (int x = 0; x < r.w; x++) {
    Color col = mix(left, right, uint8_t(r.w <= 1 ? 0 : x * 255 / (r.w - 1)));
    c.fillRect(r.x + x, r.y, 1, r.h, col);
  }
}

void circle(Canvas& c, int cx, int cy, int radius, Color col, bool filled) {
  int r2 = radius * radius;
  for (int y = -radius; y <= radius; y++) {
    int span = int(sqrtf(float(r2 - y * y)));
    if (filled) c.fillRect(cx - span, cy + y, span * 2 + 1, 1, col);
    else {
      c.set(cx - span, cy + y, col);
      c.set(cx + span, cy + y, col);
    }
  }
}
void ring(Canvas& c, int cx, int cy, int radius, int thick, Color col) {
  int r2o = radius * radius, r2i = (radius - thick) * (radius - thick);
  for (int y = -radius; y <= radius; y++)
    for (int x = -radius; x <= radius; x++) {
      int d = x * x + y * y;
      if (d <= r2o && d >= r2i) c.set(cx + x, cy + y, col);
    }
}

void tri(Canvas& c, int x0, int y0, int x1, int y1, int x2, int y2, Color col) {
  int minY = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
  int maxY = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
  for (int y = minY; y <= maxY; y++) {
    int xs[3], n = 0;
    int px[3] = {x0, x1, x2}, py[3] = {y0, y1, y2};
    for (int i = 0; i < 3; i++) {
      int j = (i + 1) % 3;
      if ((py[i] <= y && py[j] > y) || (py[j] <= y && py[i] > y)) {
        float t = float(y - py[i]) / float(py[j] - py[i]);
        xs[n++] = int(px[i] + t * (px[j] - px[i]));
      }
    }
    if (n >= 2) {
      int a = xs[0] < xs[1] ? xs[0] : xs[1];
      int b = xs[0] < xs[1] ? xs[1] : xs[0];
      c.fillRect(a, y, b - a + 1, 1, col);
    }
  }
}

void glowLine(Canvas& c, int x, int y, int w, Color col) {
  c.blendRect(x, y - 2, w, 5, col, 30);
  c.blendRect(x, y - 1, w, 3, col, 70);
  c.fillRect(x, y, w, 1, col);
}

void blit1bpp(Canvas& c, int x, int y, int w, int h, const uint8_t* bits, Color col, uint8_t alpha) {
  int stride = (w + 7) / 8;
  for (int j = 0; j < h; j++) {
    const uint8_t* row = bits + j * stride;
    for (int i = 0; i < w; i++) {
      if (row[i >> 3] & (0x80 >> (i & 7))) c.blend(x + i, y + j, col, alpha);
    }
  }
}
void blitGray4(Canvas& c, int x, int y, int w, int h, const uint8_t* nib, Color col, uint8_t alpha) {
  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      int idx = j * w + i;
      uint8_t v = (idx & 1) ? (nib[idx >> 1] & 0x0F) : (nib[idx >> 1] >> 4);
      if (!v) continue;
      c.blend(x + i, y + j, col, uint8_t(v * 17 * alpha / 255));
    }
  }
}
void blitGray8(Canvas& c, int x, int y, int w, int h, const uint8_t* gray, Color col, uint8_t alpha) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) {
      uint8_t v = gray[j * w + i];
      if (!v) continue;
      c.blend(x + i, y + j, col, uint8_t(uint16_t(v) * alpha / 255));
    }
}

// ---- 图标 ----
void iconSignal(Canvas& c, int x, int y, int size, Color col, int bars) {
  int n = 4;
  int bw = size / (n * 2 - 1);
  if (bw < 1) bw = 1;
  int gap = bw;
  for (int i = 0; i < n; i++) {
    int hh = (size * (i + 1)) / n;
    Color cc = i < bars ? col : mix(col, Color::hex(0x000000), 180);
    c.fillRect(x + i * (bw + gap), y + size - hh, bw, hh, cc);
  }
}
void iconWifi(Canvas& c, int x, int y, int size, Color col) {
  int cx = x + size / 2, cy = y + size;
  for (int k = 1; k <= 3; k++) {
    int rad = size * k / 6;
    for (int a = -60; a <= 60; a += 2) {
      float t = float(a) * 3.14159f / 180.0f;
      int px = cx + int(sinf(t) * rad), py = cy - int(cosf(t) * rad);
      c.fillRect(px, py, 2, 2, col);
    }
  }
  c.fillRect(cx - 1, cy - 3, 3, 3, col);
}
void iconBattery(Canvas& c, int x, int y, int w, int h, Color col, int percent) {
  rect(c, Rect(x, y, w, h), col, 1);
  c.fillRect(x + w, y + h / 3, 2, h / 3, col);
  int innerW = w - 4;
  int fillW = innerW * (percent < 0 ? 0 : percent > 100 ? 100 : percent) / 100;
  if (fillW > 0) c.fillRect(x + 2, y + 2, fillW, h - 4, percent <= 20 ? Color::hex(0xFF3D57) : col);
}
void iconCheck(Canvas& c, int x, int y, int size, Color col) {
  int t = size / 8 + 1;
  line(c, x + size / 8, y + size / 2, x + size * 2 / 5, y + size * 4 / 5, col, t);
  line(c, x + size * 2 / 5, y + size * 4 / 5, x + size * 7 / 8, y + size / 6, col, t);
}
void iconCross(Canvas& c, int x, int y, int size, Color col) {
  int t = size / 8 + 1;
  line(c, x + size / 5, y + size / 5, x + size * 4 / 5, y + size * 4 / 5, col, t);
  line(c, x + size * 4 / 5, y + size / 5, x + size / 5, y + size * 4 / 5, col, t);
}
void iconChevron(Canvas& c, int x, int y, int size, Color col, bool right) {
  int t = size / 6 + 1;
  if (right) {
    line(c, x + size / 3, y + size / 5, x + size * 2 / 3, y + size / 2, col, t);
    line(c, x + size * 2 / 3, y + size / 2, x + size / 3, y + size * 4 / 5, col, t);
  } else {
    line(c, x + size * 2 / 3, y + size / 5, x + size / 3, y + size / 2, col, t);
    line(c, x + size / 3, y + size / 2, x + size * 2 / 3, y + size * 4 / 5, col, t);
  }
}
void iconGear(Canvas& c, int cx, int cy, int radius, Color col) {
  ring(c, cx, cy, radius, radius / 3, col);
  for (int i = 0; i < 8; i++) {
    float a = i * 3.14159f / 4.0f;
    int x0 = cx + int(cosf(a) * radius), y0 = cy + int(sinf(a) * radius);
    int x1 = cx + int(cosf(a) * (radius + radius / 2)), y1 = cy + int(sinf(a) * (radius + radius / 2));
    line(c, x0, y0, x1, y1, col, radius / 3 + 1);
  }
}
void iconThermo(Canvas& c, int x, int y, int h, Color col) {
  int w = h / 5 + 2;
  fillRound(c, Rect(x, y, w, h - w / 2), w / 2, col);
  circle(c, x + w / 2, y + h - w / 2, w / 2 + 1, col, true);
  c.fillRect(x + w / 2 - 1, y + h / 3, 2, h / 2, mix(col, Color::hex(0x000000), 120));
}
void iconDrop(Canvas& c, int cx, int cy, int size, Color col) {
  for (int y = -size / 2; y <= size / 2; y++) {
    int span = (size / 2) - iabs(y);
    if (y < 0) span = size / 2 + y;
    if (span > 0) c.fillRect(cx - span, cy + y, span * 2, 1, col);
  }
  circle(c, cx, cy + size / 4, size / 3, col, true);
}

}  // namespace gfx
}  // namespace ui
