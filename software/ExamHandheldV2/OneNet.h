#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <mbedtls/base64.h>
#include <mbedtls/md.h>
#include "Sensors.h"
#include "Actuators.h"
#include "LinkageCore.h"
#include "CloudSet.h"
#include "Board.h"
#include "OneNetTransport.h"

// 任务 B · OneNET 接入
//
// 与任务 A 共用同一套传感器与执行器：Sensors 提供温度/湿度/两路模拟量，
// Actuators 提供电机与舵机。本类只负责「算 token → 连 MQTT → 发属性 → 收下发」。
//
// 鉴权要点（踩坑记录见 docs/踩坑与经验总结-v1.md 第 9/10 条）：
//   password 不是设备密钥，而是用密钥签名出的 token；
//   token 里的 et 是绝对过期时间，所以必须先 NTP 对时，否则必然 CONNACK 4。
static String onenetEscape(const String& text) {
  String out;
  const char hex[] = "0123456789ABCDEF";
  for (size_t i = 0; i < text.length(); ++i) {
    const unsigned char c = text[i];
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-' || c == '.' || c == '_' || c == '~') out += char(c);
    else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
  }
  return out;
}
class OneNet {
 public:
  void suspend() { if (mqtt_.connected()) mqtt_.disconnect(); nextConnect_ = 0; }
  bool connected() { return mqtt_.connected(); }
  uint32_t lastReportMs() const { return lastReport_; }
  uint32_t downlinkCount() const { return downlinkCount_; }
  int fanLevel() const { return fanLevel_; }
  int servoAngle() const { return servoAngle_; }
  bool manualHold() const { return manualHold_ && millis() - manualAt_ < kManualHoldMs; }

  void configure(const String& pid, const String& dev, const String& key,
                 const String& host = "mqtts.heclouds.com", uint16_t port = onetransport::Port) {
    suspend();
    onetransport::prepare(net_);
    pid_ = pid; dev_ = dev; key_ = key; host_ = host; port_ = port;
    ready_ = !pid_.isEmpty() && !dev_.isEmpty() && !key_.isEmpty();
    if (ready_) {
      const String base = "$sys/" + pid_ + "/" + dev_;
      topicUp_ = base + "/thing/property/post";
      topicSet_ = base + "/thing/property/set";
      topicReply_ = base + "/thing/property/set_reply";
      mqtt_.setServer(host_.c_str(), port_);
      mqtt_.setBufferSize(1024);
      mqtt_.setKeepAlive(60);
      mqtt_.setCallback([this](char* t, uint8_t* p, unsigned int n) { onMessage(t, p, n); });
    }
  }
  bool configured() const { return ready_; }

  // 每轮主循环调用：维持连接、按周期上报、执行联动
  void tick(const Sensors& s, Actuators& act) {
    if (!ready_) return;
    linkage(s, act);
    if (WiFi.status() != WL_CONNECTED) { if (mqtt_.connected()) mqtt_.disconnect(); return; }
    if (mqtt_.connected()) { mqtt_.loop(); }
    else if (int32_t(millis() - nextConnect_) >= 0) {
      nextConnect_ = millis() + 5000;
      const String token = makeToken();
      if (token.isEmpty()) return;  // 未对时或签名失败，下一轮再试
      if (mqtt_.connect(dev_.c_str(), pid_.c_str(), token.c_str())) {
        Serial.println("[onenet] connected");
        mqtt_.subscribe(topicSet_.c_str(), 1);
      } else {
        Serial.printf("[onenet] connect fail state=%d\n", mqtt_.state());
      }
    }
    if (mqtt_.connected() && millis() - lastReport_ >= kReportIntervalMs) {
      lastReport_ = millis();
      report(s);
    }
  }

 private:
  static constexpr uint32_t kReportIntervalMs = 10000;
  static constexpr uint32_t kManualHoldMs = 30000;
  // 联动阈值，与 BCfg.h 保持一致

  onetransport::Client net_;
  PubSubClient mqtt_{net_};
  String pid_, dev_, key_, host_ = "mqtts.heclouds.com", topicUp_, topicSet_, topicReply_;
  uint16_t port_ = onetransport::Port;
  bool ready_ = false;
  uint32_t nextConnect_ = 0, lastReport_ = 0, msgSeq_ = 0, downlinkCount_ = 0;
  int fanLevel_ = 0, servoAngle_ = 0;
  bool manualHold_ = false;
  uint32_t manualAt_ = 0;

  String makeToken(uint32_t ttlSec = 3600u) {
    unsigned char keyBuf[192];
    size_t keyLen = 0;
    if (mbedtls_base64_decode(keyBuf, sizeof(keyBuf), &keyLen,
                              (const unsigned char*)key_.c_str(), key_.length()) != 0) return String();
    const time_t now = time(nullptr);
    if (now < 1700000000) return String();  // 没对时就不发，见踩坑第 10 条
    const uint32_t et = uint32_t(now) + ttlSec;
    const String res = "products/" + pid_ + "/devices/" + dev_;
    const String forSign = String(et) + "\nsha1\n" + res + "\n2018-10-31";

    unsigned char mac[20];
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    const bool ok = mbedtls_md_setup(&ctx, info, 1) == 0 &&
                    mbedtls_md_hmac_starts(&ctx, keyBuf, keyLen) == 0 &&
                    mbedtls_md_hmac_update(&ctx, (const unsigned char*)forSign.c_str(),
                                           forSign.length()) == 0 &&
                    mbedtls_md_hmac_finish(&ctx, mac) == 0;
    mbedtls_md_free(&ctx);
    if (!ok) return String();

    unsigned char signBuf[64];
    size_t signLen = 0;
    if (mbedtls_base64_encode(signBuf, sizeof(signBuf), &signLen, mac, sizeof(mac)) != 0) return String();
    return "version=2018-10-31&res=" + onenetEscape(res) + "&et=" + String(et) + "&method=sha1&sign=" +
           onenetEscape(String((const char*)signBuf, signLen));
  }

  int smokePpm(const Sensors& s) const {
    return board::kHasMq ? s.mq[0].gasIndex() : -1;
  }
  int airPpm(const Sensors& s) const {
    return board::kHasMq ? s.mq[1].gasIndex() : -1;
  }

  void linkage(const Sensors& s, Actuators& act) {
    const bool hold = manualHold_ && millis() - manualAt_ < kManualHoldMs;
    if (hold) return;
    manualHold_ = false;

    // 决策逻辑抽在 LinkageCore.h，主机侧用同一份头文件做模拟验证
    const linkage::Output want = linkage::decide(s.dhtValid, s.temperature, smokePpm(s),
                                                 airPpm(s), servoAngle_);
    if (want.fanPct != fanLevel_) {
      ActuatorCommand c;
      c.fields = ActuatorCommand::MotorA;
      c.motorAPct = want.fanPct;
      if (!act.apply(c)) return;
      Serial.printf("[link] fan %d%% (temp %.1f smoke %d)\n", want.fanPct, s.temperature, smokePpm(s));
      fanLevel_ = want.fanPct;
    }
    if (want.servoDeg != servoAngle_) {
      ActuatorCommand c;
      c.fields = ActuatorCommand::ServoPosition | ActuatorCommand::ServoEnabled;
      c.servoPosition = cloudset::normalizedPosition(want.servoDeg);
      c.servoEnabled = true;
      if (!act.apply(c)) return;
      Serial.printf("[link] servo %d deg (air %d)\n", want.servoDeg, airPpm(s));
      servoAngle_ = want.servoDeg;
    }
  }

  void report(const Sensors& s) {
    JsonDocument d;
    d["id"] = String(++msgSeq_);
    d["version"] = "1.0";
    JsonObject p = d["params"].to<JsonObject>();
    if (s.dhtValid) {
      p["temperature"]["value"] = roundf(s.temperature * 10) / 10.0f;
      p["humidity"]["value"] = roundf(s.humidity * 10) / 10.0f;
    }
    const int smoke = smokePpm(s);
    if (smoke >= 0) p["smoke"]["value"] = smoke;
    const int air = airPpm(s);
    if (air >= 0) p["air"]["value"] = air;
    if (act_ && act_->ready()) {
      p["fan_level"]["value"] = act_->speedPercent(false);
      if (act_->pulseWidth()) p["servo"]["value"] = (act_->servoPosition() + 100) * 180 / 200;
    }
    String out;
    serializeJson(d, out);
    if (!mqtt_.publish(topicUp_.c_str(), out.c_str(), false)) Serial.println("[onenet] publish failed");
  }

  // 云端下发：{"id":"1","params":{"servo":{"value":90}}}
  void onMessage(char* topic, uint8_t* payload, unsigned int len) {
    if (topicSet_ != String(topic)) return;
    JsonDocument d;
    if (deserializeJson(d, payload, len)) return;
    cloudset::Command request;
    bool valid = cloudset::parse(d["params"].as<JsonObjectConst>(), request);
    ActuatorCommand c;
    if (valid && request.fan >= 0) { c.fields |= ActuatorCommand::MotorA; c.motorAPct = request.fan; }
    if (valid && request.servo >= 0) {
      c.fields |= ActuatorCommand::ServoPosition | ActuatorCommand::ServoEnabled;
      c.servoPosition = cloudset::normalizedPosition(request.servo);
      c.servoEnabled = true;
    }
    bool applied = valid && act_ && act_->apply(c);
    if (applied) {
      if (request.fan >= 0) fanLevel_ = request.fan;
      if (request.servo >= 0) servoAngle_ = request.servo;
      manualHold_ = true;
      manualAt_ = millis();
      ++downlinkCount_;
    }
    JsonDocument r;
    r["id"] = d["id"];
    r["code"] = applied ? 200 : (valid ? 503 : 400);
    r["msg"] = applied ? "success" : (valid ? "actuator unavailable" : "invalid properties");
    String out;
    serializeJson(r, out);
    mqtt_.publish(topicReply_.c_str(), out.c_str(), false);
  }

 public:
  void attach(Actuators* a) { act_ = a; }

 private:
  Actuators* act_ = nullptr;
};
