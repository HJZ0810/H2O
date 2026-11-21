#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "Sensors.h"
#include "Actuators.h"
#include "Board.h"

// 镜像通道：把同一份传感器数据同时发到自建 broker，供网页面板实时显示与下发。
//
// 为什么不直接用 OneNET 给网页用：OneNET 不把设备上报的属性转发给别的订阅者，
// 而网页若用设备 clientId 连接又会把板子挤下线。所以板子额外连一路自建 broker，
// 面板订阅这路数据，控制指令也从这里回设备。
class Mirror {
 public:
  void configure(const String& dev, const String& host, uint16_t port,
                 const String& user, const String& pass) {
    dev_ = dev; host_ = host; port_ = port; user_ = user; pass_ = pass;
    topicTele_ = "h2o/" + dev_ + "/telemetry";
    topicCmd_ = "h2o/" + dev_ + "/cmd";
    mqtt_.setServer(host_.c_str(), port_);
    mqtt_.setBufferSize(512);
    mqtt_.setCallback([this](char* t, uint8_t* p, unsigned n) { onMessage(t, p, n); });
    ready_ = !dev_.isEmpty() && !host_.isEmpty();
  }

  bool connected() { return mqtt_.connected(); }

  void tick(const Sensors& s, Actuators& act, int fanPct, int servoDeg, bool onenetUp) {
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
        mqtt_.subscribe(topicCmd_.c_str(), 1);
      } else {
        Serial.printf("[mirror] fail state=%d\n", mqtt_.state());
      }
    }
    if (mqtt_.connected() && millis() - lastReport_ >= kIntervalMs) {
      lastReport_ = millis();
      publish(s, act, fanPct, servoDeg, onenetUp);
    }
  }

 private:
  static constexpr uint32_t kIntervalMs = 5000;
  WiFiClient net_;
  PubSubClient mqtt_{net_};
  String dev_, host_, user_, pass_, topicTele_, topicCmd_;
  uint16_t port_ = 1884;
  bool ready_ = false;
  uint32_t nextConnect_ = 0, lastReport_ = 0;

  static int ppm(const AnalogSample& a) {
    return (a.enabled && a.sampled) ? int(a.outputMv() * 1000.0f / 3300.0f) : -1;
  }
  // 通道在配置里被关掉时（验证板只接 MQ1），直接读引脚兜底，
  // 否则面板上烟雾永远是"--"，看起来像没数据。
  static int ppmPin(int pin) {
    if (pin < 0) return -1;
    return int(analogReadMilliVolts(pin) * 1000.0f / 3300.0f);
  }

  void publish(const Sensors& s, Actuators& act, int fanPct, int servoDeg, bool onenetUp) {
    JsonDocument d;
    if (s.dhtValid) {
      d["temperature"] = roundf(s.temperature * 10) / 10.0f;
      d["humidity"] = roundf(s.humidity * 10) / 10.0f;
    }
    int smoke = ppm(s.mq[0]);
    if (smoke < 0) smoke = ppmPin(board::MQ1);
    if (smoke >= 0) d["smoke"] = smoke;
    int air = ppm(s.mq[1]);
    if (air < 0) air = ppmPin(board::MQ2);
    if (air >= 0) d["air"] = air;
    d["fan_level"] = fanPct;
    d["servo"] = servoDeg;
    d["ts"] = millis();
    d["onenet"] = onenetUp ? 1 : 0;
    String out;
    serializeJson(d, out);
    mqtt_.publish(topicTele_.c_str(), out.c_str(), false);
  }

  // 面板下发：{"servo":90} / {"fan_level":50}
  void onMessage(char* topic, uint8_t* payload, unsigned int len) {
    if (topicCmd_ != String(topic)) return;
    JsonDocument d;
    if (deserializeJson(d, payload, len)) return;
    ActuatorCommand c;
    if (d["servo"].is<int>()) {
      c.fields |= ActuatorCommand::ServoPosition | ActuatorCommand::ServoEnabled;
      c.servoPosition = d["servo"].as<int>();
      c.servoEnabled = true;
    }
    if (d["fan_level"].is<int>()) {
      c.fields |= ActuatorCommand::MotorA;
      c.motorAPct = d["fan_level"].as<int>();
    }
    if (c.fields) {
      act_->apply(c);
      Serial.printf("[mirror] cmd servo=%d fan=%d\n", c.servoPosition, c.motorAPct);
    }
  }

 public:
  void attach(Actuators* a) { act_ = a; }

 private:
  Actuators* act_ = nullptr;
};
