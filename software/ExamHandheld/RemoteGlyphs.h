#pragma once

#include "HanGlyph.h"
#include <Arduino.h>
#include <array>

class RemoteGlyphs : public glyph::Source {
 public:
  bool begin();
  const glyph::Bitmap* lookup(uint32_t codepoint) override;
  bool poll() override;

 private:
  enum class Status : uint8_t { Retry, Pending, Ready, Missing };
  struct Entry {
    uint32_t codepoint=0, lastUse=0, retryAt=0;
    Status status=Status::Retry;
    glyph::Bitmap bitmap{};
  };
  struct Result {
    uint32_t codepoint;
    Status status;
    glyph::Bitmap bitmap{};
  };
  std::array<Entry, 96> entries_{};
  QueueHandle_t requests_=nullptr, responses_=nullptr;
  uint32_t use_=0;
  static void task(void* context);
  void run();
  Result fetch(uint32_t codepoint);
};
