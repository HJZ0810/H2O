#include "HanRaster.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

struct Point {
  int16_t x;
  int16_t y;
  bool onCurve;
};

static void append16(std::vector<uint8_t>& bytes, uint16_t value) {
  bytes.push_back(value & 0xff);
  bytes.push_back(value >> 8);
}

static std::vector<uint8_t> outline(const std::vector<std::vector<Point>>& contours) {
  std::vector<uint8_t> bytes = {'H', 'G', 'O', 1};
  size_t count = 0;
  for (const auto& contour : contours) count += contour.size();
  append16(bytes, contours.size());
  append16(bytes, count);
  size_t end = 0;
  for (const auto& contour : contours) {
    end += contour.size();
    append16(bytes, end - 1);
  }
  for (const auto& contour : contours) {
    for (const auto& point : contour) {
      append16(bytes, static_cast<uint16_t>(point.x));
      append16(bytes, static_cast<uint16_t>(point.y));
      bytes.push_back(point.onCurve ? 1 : 0);
    }
  }
  return bytes;
}

static uint8_t pixel(const glyph::Bitmap& bitmap, int x, int y) {
  return glyph::coverage(bitmap, x, y);
}

int main() {
  glyph::Bitmap bitmap{};
  const auto lShape = outline({{{0,0,true},{700,0,true},{700,200,true},
                               {200,200,true},{200,700,true},{0,700,true}}});
  assert(glyph::rasterizeOutline(lShape.data(), lShape.size(), bitmap));
  assert(pixel(bitmap, 1, 4) > 10);
  assert(pixel(bitmap, 6, 4) == 0);
  assert(pixel(bitmap, 6, 9) > 10);

  const auto hole = outline({{{0,0,true},{900,0,true},{900,850,true},{0,850,true}},
                             {{300,250,true},{300,600,true},{650,600,true},{650,250,true}}});
  assert(glyph::rasterizeOutline(hole.data(), hole.size(), bitmap));
  assert(pixel(bitmap, 1, 5) > 10);
  assert(pixel(bitmap, 5, 5) == 0);

  const auto curve = outline({{{100,0,true},{900,0,true},{900,400,false},
                                {500,800,true},{100,400,false}}});
  assert(glyph::rasterizeOutline(curve.data(), curve.size(), bitmap));
  assert(pixel(bitmap, 5, 5) > 10);
  assert(pixel(bitmap, 0, 0) == 0);
  assert(pixel(bitmap, 1, 9) > 0);
  const auto referenceCurve = bitmap;
  const auto rotatedCurve = outline({{{900,400,false},{500,800,true},{100,400,false},
                                     {100,0,true},{900,0,true}}});
  assert(glyph::rasterizeOutline(rotatedCurve.data(), rotatedCurve.size(), bitmap));
  assert(bitmap == referenceCurve);

  std::vector<Point> implied = {{0,0,true},{200,0,false},{300,0,false},{700,0,true},
                                {700,700,true},{0,700,true}};
  const auto initialImplied = outline({implied});
  assert(glyph::rasterizeOutline(initialImplied.data(), initialImplied.size(), bitmap));
  const auto referenceImplied = bitmap;
  std::rotate(implied.begin(), implied.begin() + 2, implied.end());
  const auto rotatedImplied = outline({implied});
  assert(glyph::rasterizeOutline(rotatedImplied.data(), rotatedImplied.size(), bitmap));
  assert(bitmap == referenceImplied);

  auto broken = lShape;
  assert(!glyph::rasterizeOutline(broken.data(), broken.size() - 1, bitmap));
  broken[0] = 'X';
  assert(!glyph::rasterizeOutline(broken.data(), broken.size(), bitmap));
  broken = lShape;
  broken[8] = 0xff;
  broken[9] = 0xff;
  assert(!glyph::rasterizeOutline(broken.data(), broken.size(), bitmap));
  broken = lShape;
  broken.back() = 2;
  assert(!glyph::rasterizeOutline(broken.data(), broken.size(), bitmap));
  std::cout << "TrueType outlines rasterized on device: orientation, winding, curves, malformed data PASS\n";
}
