#pragma once

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <string>

constexpr int LOW=0, HIGH=1, OUTPUT=3;
inline uint32_t simulatedMillis=0;
inline uint32_t millis() {return simulatedMillis;}

void digitalWrite(int pin, int value);
void pinMode(int pin, int mode);
bool ledcAttachChannel(int pin, uint32_t frequency, uint8_t resolution, uint8_t channel);
bool ledcWrite(int pin, uint32_t duty);
bool ledcDetach(int pin);

template<typename T>
T constrain(T value, T minimum, T maximum) {
  return std::clamp(value, minimum, maximum);
}

class String {
 public:
  String() = default;
  String(const char* value) : text_(value) {}
  explicit String(std::string value) : text_(std::move(value)) {}
  const char* c_str() const {return text_.c_str();}
  size_t length() const {return text_.length();}
  bool isEmpty() const {return text_.empty();}
  char operator[](size_t index) const {return text_[index];}
  String substring(size_t first, size_t end) const {return String(text_.substr(first,end-first));}
  void replace(const char* original,const char* replacement) {
    const std::string oldText(original),newText(replacement);
    size_t at=0;
    while ((at=text_.find(oldText,at))!=std::string::npos) {
      text_.replace(at,oldText.size(),newText);
      at+=newText.size();
    }
  }
  friend bool operator!=(const String& a,const String& b) {return a.text_!=b.text_;}
  friend String operator+(const String& a,const String& b) {return String(a.text_+b.text_);}
 private:
  std::string text_;
};
