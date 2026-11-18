#pragma once
#include <Arduino.h>
#include <atomic>
#include "Config.h"
enum class NetKind {Online,Offline,Message,Error};
struct NetEvent {NetKind kind;uint32_t epoch;char payload[1];};
class ExamNetwork {
 public:
  bool begin(const DeviceConfig& cfg);
  bool send(const std::string& command,uint32_t observedEpoch);
  void telemetry(const String& json);
  NetEvent* poll();
  bool online() const {return online_.load();}
  uint32_t epoch() const {return epoch_.load();}
  bool overflowed() {return overflow_.exchange(false);}
 private:
  struct Tx {uint32_t at,epoch;char json[2048];};
  DeviceConfig cfg_;
  QueueHandle_t rx_=nullptr,tx_=nullptr,sensor_=nullptr;
  std::atomic<bool> online_{false},overflow_{false};
  std::atomic<uint32_t> epoch_{1};
  static void task(void* self);
  void run();
  void emit(NetKind kind,const char* payload="",size_t size=0);
  void loseLink();
};
