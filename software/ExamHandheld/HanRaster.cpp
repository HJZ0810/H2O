#include "HanRaster.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace glyph {
namespace {
constexpr size_t MaxOutlinePoints = 1040;
constexpr int Samples = 4;
constexpr int CurveSegments = 4;
constexpr float UnitsPerEm = 1024.0f;
constexpr float Baseline = 10.5f;

struct Point {
  float x;
  float y;
  bool onCurve;
};

struct Edge {
  int16_t x1;
  int16_t y1;
  int16_t x2;
  int16_t y2;
};

struct Crossing {
  float x;
  int direction;
};

uint16_t read16(const uint8_t* bytes) {
  return static_cast<uint16_t>(bytes[0]) |
         (static_cast<uint16_t>(bytes[1]) << 8);
}

Point midpoint(const Point& a, const Point& b) {
  return {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, true};
}

void appendEdge(std::vector<Edge>& edges, const Point& from, const Point& to) {
  if (from.y != to.y) {
    edges.push_back({static_cast<int16_t>(std::lround(from.x)),
                     static_cast<int16_t>(std::lround(from.y)),
                     static_cast<int16_t>(std::lround(to.x)),
                     static_cast<int16_t>(std::lround(to.y))});
  }
}

void appendQuadratic(std::vector<Edge>& edges, const Point& from,
                     const Point& control, const Point& to) {
  Point previous = from;
  for (int step = 1; step <= CurveSegments; ++step) {
    const float t = static_cast<float>(step) / CurveSegments;
    const float remainder = 1.0f - t;
    const Point next{remainder * remainder * from.x + 2 * remainder * t * control.x + t * t * to.x,
                     remainder * remainder * from.y + 2 * remainder * t * control.y + t * t * to.y,
                     true};
    appendEdge(edges, previous, next);
    previous = next;
  }
}
}

bool rasterizeOutline(const uint8_t* data, size_t length, Bitmap& bitmap) {
  bitmap.fill(0);
  if (!data || length < 8 || data[0] != 'H' || data[1] != 'G' ||
      data[2] != 'O' || data[3] != 1) return false;
  const size_t contourCount = read16(data + 4);
  const size_t pointCount = read16(data + 6);
  if (!contourCount || !pointCount || pointCount > MaxOutlinePoints ||
      contourCount > pointCount || length != 8 + contourCount * 2 + pointCount * 5) return false;

  const uint8_t* endpoints = data + 8;
  const uint8_t* encodedPoints = endpoints + contourCount * 2;
  std::vector<Point> points;
  points.reserve(pointCount);
  for (size_t i = 0; i < pointCount; ++i) {
    const uint8_t* point = encodedPoints + i * 5;
    if (point[4] > 1) return false;
    points.push_back({static_cast<float>(static_cast<int16_t>(read16(point))),
                      static_cast<float>(static_cast<int16_t>(read16(point + 2))),
                      point[4] == 1});
  }

  std::vector<Edge> edges;
  edges.reserve(pointCount * 2);
  size_t first = 0;
  for (size_t contour = 0; contour < contourCount; ++contour) {
    const size_t last = read16(endpoints + contour * 2);
    if (last < first || last >= pointCount) return false;
    Point start;
    size_t current = first, end = last + 1;
    if (points[first].onCurve) {
      start = points[first];
      ++current;
    } else if (points[last].onCurve) {
      start = points[last];
      --end;
    } else {
      start = midpoint(points[last], points[first]);
    }
    Point previous = start;
    while (current < end) {
      const Point point = points[current++];
      if (point.onCurve) {
        appendEdge(edges, previous, point);
        previous = point;
        continue;
      }
      Point next = start;
      if (current < end) {
        next = points[current];
        if (next.onCurve) ++current;
        else next = midpoint(point, next);
      }
      appendQuadratic(edges, previous, point, next);
      previous = next;
    }
    appendEdge(edges, previous, start);
    first = last + 1;
  }
  if (first != pointCount) return false;

  std::array<uint8_t, Side * Side> samples{};
  std::vector<Crossing> crossings;
  for (int row = 0; row < Side * Samples; ++row) {
    crossings.clear();
    const float y = (Baseline - (row + 0.5f) / Samples) * UnitsPerEm / Side;
    for (const auto& edge : edges) {
      const bool upward = edge.y1 <= y && edge.y2 > y;
      const bool downward = edge.y2 <= y && edge.y1 > y;
      if (!upward && !downward) continue;
      const float x = edge.x1 + (y - edge.y1) *
          (edge.x2 - edge.x1) / (edge.y2 - edge.y1);
      crossings.push_back({x, upward ? 1 : -1});
    }
    std::sort(crossings.begin(), crossings.end(),
              [](const Crossing& a, const Crossing& b) { return a.x < b.x; });
    size_t crossing = 0;
    int winding = 0;
    for (int column = 0; column < Side * Samples; ++column) {
      const float x = (column + 0.5f) * UnitsPerEm / (Side * Samples);
      while (crossing < crossings.size() && crossings[crossing].x <= x) {
        winding += crossings[crossing++].direction;
      }
      if (winding) ++samples[(row / Samples) * Side + column / Samples];
    }
  }
  // A slightly brighter coverage curve preserves thin strokes on the small LCD.
  constexpr uint8_t alpha[] = {0, 4, 5, 6, 8, 8, 9, 10, 11, 11, 12, 12, 13, 14, 14, 15, 15};
  for (size_t i = 0; i < samples.size(); i += 2) {
    bitmap[i / 2] = (alpha[samples[i]] << 4) | alpha[samples[i + 1]];
  }
  return true;
}
}
