#pragma once

#include "HanGlyph.h"

namespace glyph {
// Renders a single HGO1 TrueType outline into the display's 12x12 alpha bitmap.
bool rasterizeOutline(const uint8_t* data, size_t length, Bitmap& bitmap);
}
