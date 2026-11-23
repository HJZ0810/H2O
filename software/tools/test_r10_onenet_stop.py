"""Offline regression of handheld key/serial stop, OneNet and vendored MQTT.

Run with python software/tools/test_r10_onenet_stop.py [path/to/g++].
The broker, GPIO, serial, NVS and token signing are local stand-ins.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

SOFTWARE = Path(__file__).resolve().parents[1]
SKETCH = SOFTWARE / 'ExamHandheldV2'
LIBRARIES = SOFTWARE / 'libraries'

STUBS = {
    'Arduino.h': r'''
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <type_traits>
#include <utility>
using boolean = bool;
using byte = uint8_t;
inline uint32_t clockMs = 40000;
inline uint32_t millis() { return clockMs; }
inline void yield() { ++clockMs; }
#define PROGMEM
#define pgm_read_byte_near(p) (*(p))
class String : public std::string {
public:
  using std::string::string;
  String() = default;
  String(const std::string& s) : std::string(s) {}
  template<class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
  explicit String(T n) : std::string(std::to_string(n)) {}
  size_t write(uint8_t b) { push_back(char(b)); return 1; }
  size_t write(const uint8_t* b, size_t n) { append(reinterpret_cast<const char*>(b), n); return n; }
  bool isEmpty() const { return empty(); }
  bool startsWith(const char* p) const { return rfind(p, 0) == 0; }
  String substring(size_t start, size_t end = npos) const { return substr(start, end - start); }
  int indexOf(char c) const { return int(find(c)); }
  int lastIndexOf(char c) const { return int(rfind(c)); }
  void trim() {
    const auto start = find_first_not_of(" \r\n\t");
    *this = start == npos ? "" : substr(start, find_last_not_of(" \r\n\t") - start + 1);
  }
};
inline struct SerialStub {
  std::deque<int> input;
  int available() { return int(input.size()); }
  int read() { int c = input.front(); input.pop_front(); return c; }
  template<class... T> void print(T&&...) {}
  template<class... T> void println(T&&...) {}
  template<class... T> void printf(T&&...) {}
  template<class... T> void write(T&&...) {}
} Serial;
constexpr int LOW = 0, HIGH = 1, OUTPUT = 1;
inline unsigned pwm[64], level[64];
inline void pinMode(int, int) {}
inline void digitalWrite(int pin, int value) { level[pin] = value; }
inline bool ledcAttachChannel(int, int, int, int) { return true; }
inline bool ledcWrite(int pin, unsigned duty) { pwm[pin] = duty; return true; }
inline void ledcDetach(int) {}
template<class T> T constrain(T v, T lo, T hi) { return v < lo ? lo : v > hi ? hi : v; }
''',
    'IPAddress.h': '#pragma once\nstruct IPAddress { IPAddress() = default; IPAddress(unsigned char, unsigned char, unsigned char, unsigned char) {} };\n',
    'Stream.h': r'''
#pragma once
#include <Arduino.h>
class Print {
public:
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t*, size_t) = 0;
};
class Stream : public Print {};
''',
    'Client.h': r'''
#pragma once
#include <IPAddress.h>
#include <Stream.h>
class Client : public Stream {
public:
  virtual int connect(const char*, uint16_t) = 0;
  virtual int connect(IPAddress, uint16_t) = 0;
  virtual uint8_t connected() = 0;
  virtual void stop() = 0;
  virtual void flush() = 0;
  virtual int available() = 0;
  virtual int read() = 0;
};
''',
    'WiFi.h': r'''
#pragma once
#include <Client.h>
#include <vector>
constexpr int WL_CONNECTED = 3;
inline struct WiFiStub { bool online = false; int status() { return online ? WL_CONNECTED : 0; } } WiFi;
namespace broker {
enum Mode { Accept, Reject, ShortWrite, Silence, WrongId };
inline Mode mode = Accept;
inline bool up = false;
inline uint16_t subscriptionId = 0;
inline int connects = 0, subscriptions = 0, reports = 0, replies = 0;
inline std::deque<uint8_t> rx;
inline void push(const std::vector<uint8_t>& bytes) { rx.insert(rx.end(), bytes.begin(), bytes.end()); }
}
class WiFiClient : public Client {
public:
  int connect(const char*, uint16_t) override { return connect(IPAddress(), 0); }
  int connect(IPAddress, uint16_t) override {
    ++broker::connects;
    broker::up = WiFi.online;
    return broker::up;
  }
  uint8_t connected() override { return broker::up; }
  void stop() override { broker::up = false; broker::rx.clear(); }
  void flush() override {}
  int available() override { return int(broker::rx.size()); }
  int read() override {
    if (!available()) return -1;
    const int b = broker::rx.front(); broker::rx.pop_front(); return b;
  }
  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t* bytes, size_t len) override {
    using namespace broker;
    if (!up || !len) return 0;
    switch (bytes[0] >> 4) {
      case 1: push({0x20, 2, 0, 0}); break;
      case 8: {
        ++subscriptions;
        size_t i = 1;
        while (bytes[i++] & 0x80) {}
        subscriptionId = (bytes[i] << 8) | bytes[i + 1];
        if (mode == ShortWrite) return len - 1;
        if (mode != Silence) push({0x90, 3, bytes[i], uint8_t(bytes[i + 1] + (mode == WrongId)),
                                   uint8_t(mode == Reject ? 0x80 : 1)});
        break;
      }
      case 3: {
        const std::string packet(reinterpret_cast<const char*>(bytes), len);
        if (packet.find("/post") != std::string::npos) ++reports;
        if (packet.find("/set_reply") != std::string::npos) ++replies;
        break;
      }
    }
    return len;
  }
};
''',
    'WiFiClientSecure.h': '#pragma once\n#include <WiFi.h>\nclass WiFiClientSecure : public WiFiClient { public: void setCACert(const char*) {} };\n',
    'DHT.h': '#pragma once\nconstexpr int DHT11 = 11;\nstruct DHT { DHT(int, int) {} };\n',
    'WebServer.h': '#pragma once\nstruct WebServer { explicit WebServer(int) {} };\n',
    'Preferences.h': r'''
#pragma once
struct Preferences {
  void begin(const char*, bool) {}
  void end() {}
  void putString(const char*, const String&) {}
  String getString(const char*, const char* fallback) { return fallback; }
};
''',
    'mbedtls/base64.h': r'''
#pragma once
inline int mbedtls_base64_decode(unsigned char* b, size_t, size_t* n, const unsigned char*, size_t) {
  b[0] = 0; *n = 1; return 0;
}
inline int mbedtls_base64_encode(unsigned char* b, size_t, size_t* n, const unsigned char*, size_t) {
  b[0] = 'A'; *n = 1; return 0;
}
''',
    'mbedtls/md.h': r'''
#pragma once
constexpr int MBEDTLS_MD_SHA1 = 1;
struct mbedtls_md_info_t {};
struct mbedtls_md_context_t {};
inline const mbedtls_md_info_t* mbedtls_md_info_from_type(int) { return nullptr; }
inline void mbedtls_md_init(mbedtls_md_context_t*) {}
inline void mbedtls_md_free(mbedtls_md_context_t*) {}
inline int mbedtls_md_setup(mbedtls_md_context_t*, const mbedtls_md_info_t*, int) { return 0; }
inline int mbedtls_md_hmac_starts(mbedtls_md_context_t*, const unsigned char*, size_t) { return 0; }
inline int mbedtls_md_hmac_update(mbedtls_md_context_t*, const unsigned char*, size_t) { return 0; }
inline int mbedtls_md_hmac_finish(mbedtls_md_context_t*, unsigned char*) { return 0; }
''',
}


def between(source, start, end):
    return source[source.index(start):source.index(end, source.index(start))]


if __name__ == '__main__':
    compiler = sys.argv[1] if len(sys.argv) > 1 else os.environ.get('CXX', 'g++')
    ino = (SKETCH / 'ExamHandheldV2.ino').read_text(encoding='utf-8')
    with tempfile.TemporaryDirectory() as directory:
        tmp = Path(directory)
        for name, text in STUBS.items():
            path = tmp / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding='utf-8')
        ui = between(ino, 'static void localStop() {', '\nvoid setup() {')
        loop = between(ino, 'void loop() {', '\n  mirror.tick(')
        (tmp / 'entry.inc').write_text(ui + '\n#include "UiDebug.h"\n' + loop + '\n}\n', encoding='utf-8')
        mqtt = LIBRARIES / 'PubSubClient' / 'src'
        binary = tmp / 'test_r10_onenet_stop.exe'
        subprocess.run([compiler, '-std=c++17', '-static', '-DESP32', '-DARDUINOJSON_ENABLE_ARDUINO_STRING=0',
                        *[f'-I{p}' for p in (tmp, SKETCH, mqtt, LIBRARIES / 'ArduinoJson' / 'src')],
                        str(Path(__file__).with_suffix('.cpp')), str(SKETCH / 'Actuators.cpp'),
                        str(SKETCH / 'ExamCore.cpp'), str(mqtt / 'PubSubClient.cpp'), '-o', str(binary)], check=True)
        failures = []
        for test in ('reentry', 'menu', 'offline', 'missing-sensor', 'short-write', 'reject', 'timeout', 'wrong-id', 'early'):
            result = subprocess.run([str(binary), test], env=os.environ)
            if result.returncode:
                failures.append(f'{test} ({result.returncode})')
        if failures:
            sys.exit('Failed: ' + ', '.join(failures))
