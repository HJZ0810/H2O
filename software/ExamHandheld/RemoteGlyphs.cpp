#include "RemoteGlyphs.h"
#include "Board.h"
#include "HanRaster.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <algorithm>
#include <vector>

namespace {
uint32_t read32(const uint8_t* bytes) {
  return uint32_t(bytes[0]) | uint32_t(bytes[1]) << 8 |
         uint32_t(bytes[2]) << 16 | uint32_t(bytes[3]) << 24;
}

bool readRange(uint32_t start, uint32_t length, uint8_t* output) {
  if (!length || start >= glyph::OutlineAtlasSize ||
      length > glyph::OutlineAtlasSize - start) return false;
  WiFiClient socket;
  HTTPClient http;
  http.setConnectTimeout(1500);
  http.setTimeout(3000);
  if (!http.begin(socket, board::HanOutlineUrl)) return false;
  const String range = String(start) + "-" + String(start + length - 1);
  const char* headers[] = {"Content-Range"};
  http.collectHeaders(headers, 1);
  http.addHeader("Range", "bytes=" + range);
  const int status = http.GET();
  const bool valid = status == HTTP_CODE_PARTIAL_CONTENT &&
      http.getSize() == int(length) &&
      http.header("Content-Range") == "bytes " + range + "/" + String(glyph::OutlineAtlasSize) &&
      http.getStream().readBytes(output, length) == length;
  http.end();
  return valid;
}
}

bool RemoteGlyphs::begin() {
  requests_=xQueueCreate(16,sizeof(uint32_t));
  responses_=xQueueCreate(16,sizeof(Result));
  if (requests_ && responses_ &&
      xTaskCreatePinnedToCore(task,"han-glyph",6144,this,1,nullptr,0)==pdPASS) return true;
  if (requests_) vQueueDelete(requests_);
  if (responses_) vQueueDelete(responses_);
  requests_=responses_=nullptr;
  return false;
}

const glyph::Bitmap* RemoteGlyphs::lookup(uint32_t cp) {
  if (!requests_ || !glyph::serverHan(cp)) return nullptr;
  Entry* available=nullptr;
  Entry* found=nullptr;
  for (auto& entry : entries_) {
    if (entry.codepoint==cp) {found=&entry;break;}
    if (!entry.codepoint) available=&entry;
    else if (entry.status!=Status::Pending &&
             (!available || (available->codepoint && (use_-entry.lastUse)>(use_-available->lastUse)))) {
      available=&entry;
    }
  }
  if (found) {
    found->lastUse=++use_;
    if (found->status==Status::Ready) return &found->bitmap;
    if (found->status!=Status::Retry || int32_t(millis()-found->retryAt)<0) return nullptr;
  }
  auto* slot=found ? found : available;
  if (!slot || WiFi.status()!=WL_CONNECTED || xQueueSend(requests_,&cp,0)!=pdTRUE) return nullptr;
  slot->codepoint=cp;
  slot->lastUse=++use_;
  slot->status=Status::Pending;
  return nullptr;
}

bool RemoteGlyphs::poll() {
  if (!responses_) return false;
  bool changed=false;
  Result result{};
  while (xQueueReceive(responses_,&result,0)==pdTRUE) {
    for (auto& entry : entries_) {
      if (entry.codepoint!=result.codepoint) continue;
      entry.status=result.status;
      if (result.status==Status::Ready) {entry.bitmap=result.bitmap;changed=true;}
      if (result.status==Status::Retry) entry.retryAt=millis()+5000;
      break;
    }
  }
  return changed;
}

void RemoteGlyphs::task(void* context) {
  static_cast<RemoteGlyphs*>(context)->run();
}

void RemoteGlyphs::run() {
  for (;;) {
    uint32_t codepoint=0;
    if (xQueueReceive(requests_,&codepoint,portMAX_DELAY)!=pdTRUE) continue;
    const auto result=fetch(codepoint);
    xQueueSend(responses_,&result,portMAX_DELAY);
  }
}

RemoteGlyphs::Result RemoteGlyphs::fetch(uint32_t cp) {
  Result result{cp,Status::Retry};
  if (WiFi.status()!=WL_CONNECTED) return result;
  uint8_t index[8]{};
  if (!readRange(cp * 8, sizeof(index), index)) return result;
  const uint32_t offset = read32(index);
  const uint32_t length = read32(index + 4);
  if (offset == 0 && length == 0) {
    result.status = Status::Missing;
    return result;
  }
  if (offset < glyph::OutlineIndexSize || offset >= glyph::OutlineAtlasSize || !length ||
      length > glyph::MaxOutlineBytes || length > glyph::OutlineAtlasSize - offset) return result;
  std::vector<uint8_t> outline(length);
  if (readRange(offset, length, outline.data()) &&
      glyph::rasterizeOutline(outline.data(), outline.size(), result.bitmap)) {
    result.status = Status::Ready;
  }
  return result;
}
