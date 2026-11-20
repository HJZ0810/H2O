#pragma once

#include "Config.h"
#include "ThingsCloudProtocol.h"
#include <atomic>

class ThingsCloud {
 public:
  bool begin(const CloudConfig& config);
  bool online() const { return online_.load(); }
  const char* status() const;
  int reportError() const { return reportError_.load(); }
  void update(const thingscloud::Telemetry& sample);
  bool nextCommand(ActuatorCommand& command);
  void requestReport() { reportRequested_ = true; }

 private:
  struct RemoteCommand {
    ActuatorCommand command;
    uint32_t receivedAt, generation;
  };
  enum class Link { Disabled, WaitingWifi, Connecting, Connected, Error };
  QueueHandle_t samples_ = nullptr, incoming_ = nullptr;
  CloudConfig config_;
  std::atomic<Link> link_{Link::Disabled};
  std::atomic<bool> online_{false}, reportRequested_{false};
  std::atomic<int> reportError_{0};
  std::atomic<uint32_t> generation_{0};
  static void task(void* context);
  void run();
};
