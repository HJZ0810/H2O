// 任务 B · 物联监测控制系统（OneNET）
//
// 抽签配置：DHT11 温湿度 / MQ-2 烟雾 / MQ-135 空气质量，风扇 + 舵机，OneNET。
// 三路数据每 10 秒上报一次（要求 ≥1 次/30 s）；两个执行机构可由云端下发控制；
// 联动逻辑在 ESP32 端执行：温度或烟雾超阈值自动开风扇，空气质量超阈值舵机开窗。
//
// 凭据不进代码：全部存在 NVS（命名空间 bmon），用串口命令配置。
// 首次使用在串口 115200 下依次输入：
//   cfg ssid <WiFi名>
//   cfg wifi <WiFi密码>
//   cfg pid  <产品ID>
//   cfg dev  <设备名称>
//   cfg key  <设备密钥>
//   save
//   然后复位。show 可查看当前配置（密钥只显示长度）。

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <DHT.h>
#include "OneNetAuth.h"
#include "OneNetTransport.h"
#include "BCfg.h"
#include "CloudSet.h"

// ---------------- 配置存储 ----------------
struct Creds {
  String ssid, wifi, host = "mqtts.heclouds.com", pid, dev, key;
  uint16_t port = onetransport::Port;
  bool ready() const { return !ssid.isEmpty() && !pid.isEmpty() && !dev.isEmpty() && !key.isEmpty(); }
};

static Creds creds;
static Preferences prefs;

static void loadCreds() {
  prefs.begin("bmon", true);
  creds.ssid = prefs.getString("ssid", "");
  creds.wifi = prefs.getString("wifi", "");
  creds.host = prefs.getString("host", "mqtts.heclouds.com");
  creds.port = prefs.getUShort("port", onetransport::Port);
#ifdef H2O_ONENET_CA
  creds.port = onetransport::Port;
#endif
  creds.pid = prefs.getString("pid", "");
  creds.dev = prefs.getString("dev", "");
  creds.key = prefs.getString("key", "");
  prefs.end();
}

static void saveCreds() {
  prefs.begin("bmon", false);
  prefs.putString("ssid", creds.ssid);
  prefs.putString("wifi", creds.wifi);
  prefs.putString("host", creds.host);
  prefs.putUShort("port", creds.port);
  prefs.putString("pid", creds.pid);
  prefs.putString("dev", creds.dev);
  prefs.putString("key", creds.key);
  prefs.end();
}

static void printCreds() {
  Serial.println("---- config ----");
  Serial.println("ssid = " + creds.ssid);
  Serial.println("wifi = " + String(creds.wifi.length()) + " chars");
  Serial.println("host = " + creds.host + ":" + String(creds.port));
  Serial.println("pid  = " + creds.pid);
  Serial.println("dev  = " + creds.dev);
  Serial.println("key  = " + (creds.key.isEmpty() ? String("<unset>")
                                                  : String(creds.key.length()) + " chars"));
  Serial.println("----------------");
}

// 串口配置命令
static void pollSerialConfig() {
  static String line;
  while (Serial.available()) {
    char c = char(Serial.read());
    if (c == '\r') continue;
    static bool overflow = false;
    if (c != '\n') { if (line.length() < 200) line += c; else overflow = true; continue; }
    if (overflow) { overflow = false; line = ""; Serial.println("command too long"); continue; }
    line.trim();
    if (line.isEmpty()) continue;
    String cmd = line; line = "";
    int sp1 = cmd.indexOf(' ');
    String head = sp1 < 0 ? cmd : cmd.substring(0, sp1);
    String rest = sp1 < 0 ? String() : cmd.substring(sp1 + 1);
    if (head == "cfg") {
      int sp2 = rest.indexOf(' ');
      String k = sp2 < 0 ? rest : rest.substring(0, sp2);
      String v = sp2 < 0 ? String() : rest.substring(sp2 + 1);
      if (k == "ssid") creds.ssid = v;
      else if (k == "wifi") creds.wifi = v;
      else if (k == "host") creds.host = v;
      else if (k == "port") { long port = v.toInt(); if (port <= 0 || port > 65535 || String(port) != v) { Serial.println("invalid port"); continue; } creds.port = uint16_t(port); }
      else if (k == "pid") creds.pid = v;
      else if (k == "dev") creds.dev = v;
      else if (k == "key") creds.key = v;
      else { Serial.println("unknown key: " + k); continue; }
      Serial.println("set " + k);
    } else if (head == "save") { saveCreds(); Serial.println("saved to NVS"); }
    else if (head == "show") printCreds();
    else if (head == "reboot") { Serial.println("rebooting"); delay(200); ESP.restart(); }
    else Serial.println("cmds: cfg <ssid|wifi|host|port|pid|dev|key> <value> | save | show | reboot");
  }
}

// ---------------- 传感器 ----------------
static DHT dht(bmon::Dht, DHT11);

static int readAdcPpm(int pin, float fullScale) {
  if (pin < 0) return -1;
  uint32_t sum = 0;
  const int n = 16;
  for (int i = 0; i < n; i++) sum += analogReadMilliVolts(pin);
  float mv = float(sum) / n;
  // 3.3V 量程线性折算，仅用于趋势与阈值，不是标定浓度
  float ratio = mv / 3300.0f;
  if (ratio < 0) ratio = 0;
  if (ratio > 1) ratio = 1;
  return int(ratio * fullScale);
}

static bmon::Sample sampleSensors() {
  bmon::Sample s;
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t) && !isnan(h)) { s.temp = t; s.humi = h; s.dhtOk = true; }
  s.smoke = bmon::kVerify ? -1 : readAdcPpm(bmon::Mq2Pin, bmon::MqFullScalePpm2);
  s.air = readAdcPpm(bmon::Mq135Pin, bmon::MqFullScalePpm135);
  return s;
}

// ---------------- 执行机构 ----------------
// 风扇：LEDC 20 kHz，占空比 0~100；有功率级时同时给方向脚
// ESP32 Arduino core 3.x：LEDC 改为按引脚操作，不再显式申请通道
static int fanLevel = 0;      // %
static int servoAngle = bmon::ServoClose;
static bool manualHold = false;
static uint32_t manualAt = 0;

static bool fanReady = false, servoReady = false;
static bool applyFan(int level) {
  if (level < 0) level = 0;
  if (level > 100) level = 100;
  if (!fanReady || !ledcWrite(bmon::FanPwm, uint32_t(level) * 255 / 100)) return false;
  fanLevel = level;
  if (bmon::FanIn1 >= 0) digitalWrite(bmon::FanIn1, level > 0 ? HIGH : LOW);
  if (bmon::FanIn2 >= 0) digitalWrite(bmon::FanIn2, LOW);
  return true;
}

static bool applyServo(int angle) {
  if (angle < 0) angle = 0;
  if (angle > 180) angle = 180;
  // SG90：0.5 ms ~ 2.5 ms，周期 20 ms
  uint32_t us = 500 + uint32_t(angle) * 2000 / 180;
  uint32_t duty = us * 16383u / 20000u;
  if (!servoReady || !ledcWrite(bmon::ServoPwm, duty)) return false;
  servoAngle = angle;
  return true;
}

static void initActuators() {
  fanReady = ledcAttach(bmon::FanPwm, 20000, 8);    // 风扇 20 kHz，8 位分辨率
  servoReady = ledcAttach(bmon::ServoPwm, 50, 14);    // 舵机 50 Hz，16 位分辨率
  if (bmon::FanIn1 >= 0) pinMode(bmon::FanIn1, OUTPUT);
  if (bmon::FanIn2 >= 0) pinMode(bmon::FanIn2, OUTPUT);
  applyFan(0);
  applyServo(bmon::ServoClose);
}

// ---------------- 联动 ----------------
// 返回是否因联动改变了输出，便于串口留痕
static bool runLinkage(const bmon::Sample& s) {
  if (manualHold && millis() - manualAt < bmon::ManualHoldMs) return false;
  manualHold = false;

  int wantFan = 0;
  if (s.dhtOk && s.temp >= bmon::TempFanOn) {
    float k = (s.temp - bmon::TempFanOn) / (bmon::TempFanFull - bmon::TempFanOn);
    if (k < 0) k = 0;
    if (k > 1) k = 1;
    wantFan = int(40 + k * 60);   // 30°C→40%，40°C→100%
  }
  if (s.smoke >= 0 && s.smoke >= bmon::SmokeFanOn && wantFan < 80) wantFan = 80;

  int wantServo = servoAngle;
  if (s.air >= 0 && s.air >= bmon::AirWindowOpen) wantServo = bmon::ServoOpen;
  else if (s.air >= 0) wantServo = bmon::ServoClose;

  bool changed = false;
  if (wantFan != fanLevel) {
    applyFan(wantFan);
    Serial.printf("[link] fan -> %d%% (temp %.1f, smoke %d)\n", wantFan, s.temp, s.smoke);
    changed = true;
  }
  if (wantServo != servoAngle) {
    applyServo(wantServo);
    Serial.printf("[link] servo -> %d deg (air %d)\n", wantServo, s.air);
    changed = true;
  }
  return changed;
}

// ---------------- OneNET MQTT ----------------
static onetransport::Client net;
static PubSubClient mqtt(net);
static String topicUp, topicSet, topicReply;
static uint32_t msgSeq = 0;
static uint32_t lastReport = 0;

static void buildTopics() {
  const String base = "$sys/" + creds.pid + "/" + creds.dev;
  topicUp = base + "/thing/property/post";
  topicSet = base + "/thing/property/set";
  topicReply = base + "/thing/property/set_reply";
}

static void report(const bmon::Sample& s) {
  JsonDocument d;
  d["id"] = String(++msgSeq);
  d["version"] = "1.0";
  JsonObject p = d["params"].to<JsonObject>();
  if (s.dhtOk) {
    p["temperature"]["value"] = roundf(s.temp * 10) / 10.0f;
    p["humidity"]["value"] = roundf(s.humi * 10) / 10.0f;
  }
  if (s.smoke >= 0) p["smoke"]["value"] = s.smoke;
  if (s.air >= 0) p["air"]["value"] = s.air;
  if (fanReady) p["fan_level"]["value"] = fanLevel;
  if (servoReady) p["servo"]["value"] = servoAngle;
  String out;
  serializeJson(d, out);
  bool ok = mqtt.publish(topicUp.c_str(), out.c_str(), false);
  Serial.printf("[up] %s %s\n", ok ? "ok" : "FAIL", out.c_str());
}

// 云端属性设置：{"id":"123","params":{"fan":{"value":true},"servo":{"value":45}}}
static void onMessage(char* topic, uint8_t* payload, unsigned int len) {
  if (topicSet != String(topic)) return;
  JsonDocument d;
  if (deserializeJson(d, payload, len)) { Serial.println("[down] bad json"); return; }
  cloudset::Command requested;
  const bool valid = cloudset::parse(d["params"].as<JsonObjectConst>(), requested);
  const bool ready = (requested.fan < 0 || fanReady) && (requested.servo < 0 || servoReady);
  const bool applied = valid && ready && (requested.fan < 0 || applyFan(requested.fan)) && (requested.servo < 0 || applyServo(requested.servo));
  if (applied) {
    manualHold = true;
    manualAt = millis();
  }
  JsonDocument r;
  r["id"] = d["id"];
  r["code"] = applied ? 200 : (valid ? 503 : 400);
  r["msg"] = applied ? "success" : (valid ? "actuator unavailable" : "invalid properties");
  String out;
  serializeJson(r, out);
  mqtt.publish(topicReply.c_str(), out.c_str(), false);
}

static uint32_t nextConnectAt = 0;
static void ensureMqtt() {
  if (!creds.ready() || WiFi.status() != WL_CONNECTED) { if (mqtt.connected()) mqtt.disconnect(); return; }
  if (mqtt.connected()) { mqtt.loop(); return; }
  uint32_t now = millis();
  if (int32_t(now - nextConnectAt) < 0) return;
  nextConnectAt = now + 5000;
  // OneNET：clientId = 设备名称，username = 产品ID，password = 设备密钥
  String token = onenetMakeToken(creds.pid, creds.dev, creds.key);
  if (token.isEmpty()) { nextConnectAt = now + 3000; return; }
  if (mqtt.connect(creds.dev.c_str(), creds.pid.c_str(), token.c_str())) {
    Serial.println("[mqtt] connected to OneNET");
    mqtt.subscribe(topicSet.c_str(), 1);
  } else {
    Serial.printf("[mqtt] connect FAIL state=%d (state 4 = wrong username/password)\n", mqtt.state());
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n=== B-monitor %s ===\n", bmon::BoardName);
  loadCreds();
  printCreds();

  initActuators();
  dht.begin();
  analogSetPinAttenuation(bmon::Mq2Pin, ADC_11db);
  if (bmon::Mq135Pin >= 0) analogSetPinAttenuation(bmon::Mq135Pin, ADC_11db);

  if (creds.ssid.isEmpty()) {
    Serial.println("no wifi ssid: use cfg ssid/wifi ... then save, then reboot");
    return;
  }
  // Wi-Fi 必须在主任务里启动：放进自建任务会静默失败（2026-11-21 实测）
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(creds.ssid.c_str(), creds.wifi.c_str());
  configTime(0, 0, "ntp.aliyun.com", "cn.pool.ntp.org", "pool.ntp.org");
  buildTopics();
  onetransport::prepare(net);
  mqtt.setServer(creds.host.c_str(), creds.port);
  mqtt.setBufferSize(1024);
  mqtt.setKeepAlive(60);
  mqtt.setCallback(onMessage);

}

void loop() {
  pollSerialConfig();
  ensureMqtt();

  static uint32_t lastSample = 0;
  uint32_t now = millis();
  if (now - lastSample >= 2000) {
    lastSample = now;
    bmon::Sample s = sampleSensors();
    runLinkage(s);
    if (mqtt.connected() && now - lastReport >= bmon::ReportIntervalMs) {
      lastReport = now;
      report(s);
    }
    static uint32_t lastDiag = 0;
    if (now - lastDiag >= 10000) {
      lastDiag = now;
      Serial.printf("[diag] wifi=%d mqtt=%d T=%.1f H=%.1f smoke=%d air=%d fan=%d%% servo=%d°\n",
                    int(WiFi.status()), mqtt.connected() ? 1 : 0, s.temp, s.humi,
                    s.smoke, s.air, fanLevel, servoAngle);
    }
  }
  delay(20);
}
