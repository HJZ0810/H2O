#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "Sensors.h"
#include "Actuators.h"
#include "Board.h"

// 镜像通道：把同一份传感器数据同时发到自建 broker，供网页面板实时显示。
//
// 为什么不直接用 OneNET 给网页用：OneNET 不把设备上报的属性转发给别的订阅者，
// 而网页若用设备 clientId 连接又会把板子挤下线。所以板子额外连一路自建 broker，
// 面板订阅这路数据，控制指令由 OneNET 认证通道处理。
class Mirror {
 public:
  void configure(const String& dev, const String& host, uint16_t port,
                 const String& user, const String& pass, const char* ca) {
    dev_ = dev; host_ = host; port_ = port; user_ = user; pass_ = pass;
    topicTele_ = "h2o/" + dev_ + "/telemetry";
    net_.setCACert(ca);
    mqtt_.setServer(host_.c_str(), port_);
    mqtt_.setBufferSize(512);

    ready_ = ca && *ca && !dev_.isEmpty() && !host_.isEmpty() && !user_.isEmpty() && !pass_.isEmpty();
  }

  bool connected() { return mqtt_.connected(); }

  void tick(const Sensors& s, Actuators& act, bool onenetUp) {
    if (!ready_ || WiFi.status() != WL_CONNECTED) {
      if (mqtt_.connected()) mqtt_.disconnect();
      return;
    }
    if (mqtt_.connected()) {
      mqtt_.loop();
    } else if (int32_t(millis() - nextConnect_) >= 0) {
      nextConnect_ = millis() + 5000;
      if (mqtt_.connect(("esp32-mirror-" + dev_).c_str(), user_.c_str(), pass_.c_str())) {
        Serial.println("[mirror] connected");

      } else {
        Serial.printf("[mirror] fail state=%d\n", mqtt_.state());
      }
    }
    if (mqtt_.connected() && millis() - lastReport_ >= kIntervalMs) {
      lastReport_ = millis();
      publish(s, act, onenetUp);
    }
  }

 private:
  static constexpr uint32_t kIntervalMs = 5000;
  WiFiClientSecure net_;
  PubSubClient mqtt_{net_};
  String dev_, host_, user_, pass_, topicTele_;
  uint16_t port_ = 1884;
  bool ready_ = false;
  uint32_t nextConnect_ = 0, lastReport_ = 0;

  void publish(const Sensors& s, Actuators& act, bool onenetUp) {
    JsonDocument d;
    if (s.dhtValid) {
      d["temperature"] = roundf(s.temperature * 10) / 10.0f;
      d["humidity"] = roundf(s.humidity * 10) / 10.0f;
    }
    int smoke = board::kHasMq ? s.mq[0].gasIndex() : -1;
    if (smoke >= 0) d["smoke"] = smoke;
    int air = board::kHasMq ? s.mq[1].gasIndex() : -1;
    if (air >= 0) d["air"] = air;
    if (act.ready()) {
      d["fan_level"] = act.speedPercent(false);
      if (act.pulseWidth()) d["servo"] = (act.servoPosition() + 100) * 180 / 200;
    }
    d["uptime_ms"] = millis();
    d["onenet"] = onenetUp ? 1 : 0;
    String out;
    serializeJson(d, out);
    mqtt_.publish(topicTele_.c_str(), out.c_str(), false);
  }

};
