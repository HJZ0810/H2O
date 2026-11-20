#include "ThingsCloud.h"
#include <PubSubClient.h>
#include <WiFi.h>
#include <algorithm>

namespace {
// One device uses 720 scheduled reports per day on the 1,000-message free tier.
constexpr uint32_t ReportInterval = 120000;
constexpr uint32_t MinReportInterval = 5000;
constexpr uint32_t CommandLifetime = 3000;
constexpr uint32_t MaxReconnectDelay = 60000;

bool changedOutputs(const thingscloud::Telemetry& a, const thingscloud::Telemetry& b) {
  return a.motorAPct != b.motorAPct || a.motorBPct != b.motorBPct ||
         a.servoPosition != b.servoPosition || a.servoPulseUs != b.servoPulseUs ||
         a.servoEnabled != b.servoEnabled || a.outputsReady != b.outputsReady ||
         a.dhtValid != b.dhtValid || a.mq2Valid != b.mq2Valid || a.mq135Valid != b.mq135Valid;
}
}

bool ThingsCloud::begin(const CloudConfig& config) {
  config_ = config;
  samples_ = xQueueCreate(1, sizeof(thingscloud::Telemetry));
  incoming_ = xQueueCreate(6, sizeof(RemoteCommand));
  link_ = Link::WaitingWifi;
  if (samples_ && incoming_ && xTaskCreatePinnedToCore(task, "thingscloud", 6144, this, 1, nullptr, 0) == pdPASS) {
    return true;
  }
  if (samples_) vQueueDelete(samples_);
  if (incoming_) vQueueDelete(incoming_);
  samples_ = incoming_ = nullptr;
  link_ = Link::Error;
  return false;
}

const char* ThingsCloud::status() const {
  switch (link_.load()) {
    case Link::WaitingWifi: return "等待 Wi-Fi";
    case Link::Connecting: return "连接中";
    case Link::Connected: return "在线";
    case Link::Error: return "连接失败";
    default: return "未配置";
  }
}

void ThingsCloud::update(const thingscloud::Telemetry& sample) {
  if (samples_) xQueueOverwrite(samples_, &sample);
}

bool ThingsCloud::nextCommand(ActuatorCommand& command) {
  RemoteCommand next{};
  while (incoming_ && xQueueReceive(incoming_, &next, 0) == pdTRUE) {
    if (online() && generation_.load() == next.generation &&
        uint32_t(millis() - next.receivedAt) < CommandLifetime) {
      command = next.command;
      return true;
    }
  }
  return false;
}

void ThingsCloud::task(void* context) {
  static_cast<ThingsCloud*>(context)->run();
  vTaskDelete(nullptr);
}

void ThingsCloud::run() {
  using namespace thingscloud;
  WiFiClient tcp;
  tcp.setConnectionTimeout(2500);
  PubSubClient mqtt(tcp);
  mqtt.setServer(config_.host.c_str(), 1883);
  mqtt.setKeepAlive(60);
  mqtt.setSocketTimeout(3);
  if (!mqtt.setBufferSize(MaxRequestBytes + 64)) {
    link_ = Link::Error;
    return;
  }
  String clientId = "h2o-" + WiFi.macAddress();
  clientId.replace(":", "");
  uint32_t pausedUntil = 0;
  mqtt.setCallback([this, &pausedUntil](char* topic, uint8_t* data, unsigned length) {
    if (!strcmp(topic, PushTopic)) {
      RemoteCommand request{};
      request.receivedAt = millis();
      request.generation = generation_.load();
      if (!parsePush(data, length, request.command) ||
          xQueueSend(incoming_, &request, 0) != pdTRUE) requestReport();
    } else if (!strcmp(topic, ReportResultTopic)) {
      const int result = parseReportResult(data, length);
      reportError_ = result;
      if (result == 403) pausedUntil = millis() + 15000;
      if (result == 413) pausedUntil = millis() + 1800000;
      if (result == 403 || result == 413) requestReport();
    }
  });

  uint32_t nextConnect = millis(), retryDelay = 3000, lastReport = 0;
  bool haveSample = false, sentOnConnection = false;
  Telemetry latest{}, sent{};
  auto disconnect = [&] {
    if (online_.exchange(false)) generation_.fetch_add(1);
    if (mqtt.connected()) mqtt.disconnect();
    tcp.stop();
  };
  for (;;) {
    const uint32_t now = millis();
    if (xQueueReceive(samples_, &latest, 0) == pdTRUE) haveSample = true;
    if (WiFi.status() != WL_CONNECTED) {
      if (online()) disconnect();
      link_ = Link::WaitingWifi;
      nextConnect = now;
    } else if (!mqtt.connected()) {
      if (online()) disconnect();
      if (int32_t(now - nextConnect) >= 0) {
        link_ = Link::Connecting;
        if (mqtt.connect(clientId.c_str(), config_.accessToken.c_str(), config_.projectKey.c_str()) &&
            mqtt.subscribe(PushTopic, 0) && mqtt.subscribe(ReportResultTopic, 0)) {
          generation_.fetch_add(1);
          online_ = true;
          link_ = Link::Connected;
          retryDelay = 3000;
          sentOnConnection = false;
        } else {
          disconnect();
          link_ = Link::Error;
        }
        nextConnect = millis() + retryDelay;
        if (!online()) retryDelay = std::min(MaxReconnectDelay, retryDelay * 2);
      }
    } else if (!mqtt.loop()) {
      disconnect();
      link_ = Link::Connecting;
      nextConnect = millis() + retryDelay;
    } else if (haveSample && (!pausedUntil || int32_t(now - pausedUntil) >= 0)) {
      pausedUntil = 0;
      const auto elapsed = uint32_t(now - lastReport);
      if ((!sentOnConnection || elapsed >= MinReportInterval) &&
          (!sentOnConnection || elapsed >= ReportInterval || reportRequested_.load() || changedOutputs(latest, sent))) {
        reportRequested_.exchange(false);
        const auto payload = report(latest);
        if (mqtt.publish(ReportTopic, payload.c_str())) {
          sentOnConnection = true;
          sent = latest;
          lastReport = now;
        } else {
          disconnect();
          link_ = Link::Connecting;
          nextConnect = millis() + retryDelay;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
