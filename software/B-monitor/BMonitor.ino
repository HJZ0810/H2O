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
#include "BCfg.h"

// ---------------- 配置存储 ----------------
struct Creds {
  String ssid, wifi, host = "mqtts.heclouds.com", pid, dev, key;
  uint16_t port = 1883;
  bool ready() const { return !ssid.isEmpty() && !pid.isEmpty() && !dev.isEmpty() && !key.isEmpty(); }
};

static Creds creds;
static Preferences prefs;

static void loadCreds() {
  prefs.begin("bmon", true);
  creds.ssid = prefs.getString("ssid", "");
  creds.wifi = prefs.getString("wifi", "");
  creds.host = prefs.getString("host", "mqtts.heclouds.com");
  creds.port = prefs.getUShort("port", 1883);
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
    if (c != '\n') { if (line.length() < 200) line += c; continue; }
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
      else if (k == "port") creds.port = uint16_t(v.toInt());
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
  s.smoke = readAdcPpm(bmon::Mq2Pin, bmon::MqFullScalePpm2);
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

static void applyFan(int level) {
  if (level < 0) level = 0;
  if (level > 100) level = 100;
  fanLevel = level;
  ledcWrite(bmon::FanPwm, uint32_t(level) * 255 / 100);
  if (bmon::FanIn1 >= 0) digitalWrite(bmon::FanIn1, level > 0 ? HIGH : LOW);
  if (bmon::FanIn2 >= 0) digitalWrite(bmon::FanIn2, LOW);
}

static void applyServo(int angle) {
  if (angle < 0) angle = 0;
  if (angle > 180) angle = 180;
  servoAngle = angle;
  // SG90：0.5 ms ~ 2.5 ms，周期 20 ms
  uint32_t us = 500 + uint32_t(angle) * 2000 / 180;
  uint32_t duty = us * 65535u / 20000u;
  ledcWrite(bmon::ServoPwm, duty);
}

static void initActuators() {
  ledcAttach(bmon::FanPwm, 20000, 8);    // 风扇 20 kHz，8 位分辨率
  ledcAttach(bmon::ServoPwm, 50, 16);    // 舵机 50 Hz，16 位分辨率
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
static WiFiClient net;
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
  p["fan_level"]["value"] = fanLevel;
  p["servo"]["value"] = servoAngle;
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
  String id = d["id"] | "";
  JsonObject p = d["params"];
  if (p.isNull()) return;
  bool touched = false;
  if (p["fan"].is<JsonObject>()) {
    JsonVariant v = p["fan"]["value"];
    bool on = v.is<bool>() ? v.as<bool>() : (v.as<int>() != 0);
    applyFan(on ? 100 : 0);
    touched = true;
    Serial.printf("[down] fan=%s\n", on ? "on" : "off");
  }
  if (p["servo"].is<JsonObject>()) {
    int a = p["servo"]["value"] | servoAngle;
    applyServo(a);
    touched = true;
    Serial.printf("[down] servo=%d deg\n", a);
  }
  if (touched) { manualHold = true; manualAt = millis(); }

  JsonDocument r;
  r["id"] = id;
  r["code"] = 200;
  r["msg"] = "success";
  String out;
  serializeJson(r, out);
  mqtt.publish(topicReply.c_str(), out.c_str(), false);
}

static uint32_t nextConnectAt = 0;
static void ensureMqtt() {
  if (WiFi.status() != WL_CONNECTED) { if (mqtt.connected()) mqtt.disconnect(); return; }
  if (mqtt.connected()) { mqtt.loop(); return; }
  uint32_t now = millis();
  if (int32_t(now - nextConnectAt) < 0) return;
  nextConnectAt = now + 5000;
  // OneNET：clientId = 设备名称，username = 产品ID，password = 设备密钥
  if (mqtt.connect(creds.dev.c_str(), creds.pid.c_str(), creds.key.c_str())) {
    Serial.println("[mqtt] connected to OneNET");
    mqtt.subscribe(topicSet.c_str(), 1);
  } else {
    Serial.printf("[mqtt] connect FAIL state=%d (check pid/dev/key)\n", mqtt.state());
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
  Serial.print("[wifi] connecting");
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) { delay(500); Serial.print("."); }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[wifi] IP=");
    Serial.println(WiFi.localIP());
    if (!creds.ready()) {
      Serial.println("[mqtt] 缺 pid/dev/key：联网正常，云端待配置");
      return;
    }
    buildTopics();
    mqtt.setServer(creds.host.c_str(), creds.port);
    mqtt.setBufferSize(1024);
    mqtt.setKeepAlive(60);
    mqtt.setCallback(onMessage);
  } else {
    Serial.println("[wifi] FAIL: check ssid/password (wifi=6 cannot tell wrong pwd from out of range)");
  }
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
