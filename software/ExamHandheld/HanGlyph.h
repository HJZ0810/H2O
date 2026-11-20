#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace glyph {
constexpr uint32_t LastHan = 0x33479;
constexpr int Side = 12;
constexpr size_t Bytes = Side * Side / 2;
constexpr uint32_t AtlasSize = (LastHan + 1) * Bytes;
using Bitmap = std::array<uint8_t, Bytes>;

struct Scalar {
  uint32_t codepoint;
  size_t length;
};

inline Scalar decode(const char* text, size_t remaining) {
  if (!remaining) return {0, 0};
  const auto first = static_cast<uint8_t>(text[0]);
  if (first < 0x80) return {first, 1};
  constexpr Scalar replacement{0xFFFD, 1};
  if (first < 0xC2 || first > 0xF4) return replacement;
  const size_t count = first < 0xE0 ? 2 : first < 0xF0 ? 3 : 4;
  if (remaining < count) return replacement;
  uint32_t value = first & (0x7F >> count);
  for (size_t i = 1; i < count; ++i) {
    const auto next = static_cast<uint8_t>(text[i]);
    if ((next & 0xC0) != 0x80) return replacement;
    value = (value << 6) | (next & 0x3F);
  }
  const uint32_t minimum[] = {0, 0, 0x80, 0x800, 0x10000};
  if (value < minimum[count] || value > 0x10FFFF ||
      (value >= 0xD800 && value <= 0xDFFF)) return replacement;
  return {value, count};
}

inline bool variationSelector(uint32_t cp) {
  return (cp >= 0xFE00 && cp <= 0xFE0F) || (cp >= 0xE0100 && cp <= 0xE01EF);
}

inline bool serverHan(uint32_t cp) {
  return cp == 0x3007 || (cp >= 0x3400 && cp <= 0x4DBF) ||
         (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0xF900 && cp <= 0xFAD9) ||
         (cp >= 0x20000 && cp <= 0x2FA1D) || (cp >= 0x30000 && cp <= LastHan);
}

inline uint8_t coverage(const Bitmap& bitmap, int x, int y) {
  const uint8_t pair=bitmap[(Side * y + x) / 2];
  return x % 2 ? pair & 0x0F : pair >> 4;
}

class Source {
 public:
  virtual ~Source() = default;
  virtual const Bitmap* lookup(uint32_t cp) = 0;
  virtual bool poll() = 0;
};
}
