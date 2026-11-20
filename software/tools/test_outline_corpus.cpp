#include "HanRaster.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

static uint32_t le32(const uint8_t* data) {
  return uint32_t(data[0]) | uint32_t(data[1]) << 8 |
         uint32_t(data[2]) << 16 | uint32_t(data[3]) << 24;
}

int main(int argc, char** argv) {
  if (argc != 2 && argc != 3) {
    std::cerr << "Usage: test_outline_corpus <han-outline.bin> [sample-bitmaps.bin]\n";
    return 2;
  }
  std::ifstream file(argv[1], std::ios::binary);
  if (!file) return 2;
  std::ofstream samples;
  if (argc == 3) {
    samples.open(argv[2], std::ios::binary);
    if (!samples) return 2;
  }
  constexpr std::array<uint32_t, 6> sampleCodepoints = {
      0x867e, 0x9f98, 0x20bb7, 0x30ede, 0x323b0, 0x33479};
  size_t rendered = 0, blank = 0, malformed = 0;
  for (uint32_t cp = 0; cp <= glyph::LastHan; ++cp) {
    std::array<uint8_t, 8> index{};
    file.seekg(cp * index.size());
    file.read(reinterpret_cast<char*>(index.data()), index.size());
    if (!file) return 2;
    const uint32_t offset = le32(index.data()), length = le32(index.data() + 4);
    if (!offset && !length) continue;
    if (offset < glyph::OutlineIndexSize || offset >= glyph::OutlineAtlasSize ||
        !length || length > glyph::MaxOutlineBytes || length > glyph::OutlineAtlasSize - offset) return 1;
    std::vector<uint8_t> outline(length);
    file.seekg(offset);
    file.read(reinterpret_cast<char*>(outline.data()), outline.size());
    if (!file) return 2;
    glyph::Bitmap bitmap{};
    if (!glyph::rasterizeOutline(outline.data(), outline.size(), bitmap)) {
      if (++malformed <= 10) std::cerr << "Invalid outline U+" << std::hex << cp << std::dec << "\n";
      continue;
    }
    if (std::none_of(bitmap.begin(), bitmap.end(), [](uint8_t value) { return value != 0; })) {
      if (++blank <= 10) std::cerr << "Empty raster U+" << std::hex << cp << std::dec << "\n";
    }
    if (samples.is_open() &&
        std::find(sampleCodepoints.begin(), sampleCodepoints.end(), cp) != sampleCodepoints.end()) {
      samples.write(reinterpret_cast<const char*>(bitmap.data()), bitmap.size());
      if (!samples) return 2;
    }
    ++rendered;
  }
  std::cout << "Rendered " << rendered << " outlines; blank " << blank
            << "; invalid " << malformed << ".\n";
  return rendered == 102999 && blank == 0 && malformed == 0 ? 0 : 1;
}
