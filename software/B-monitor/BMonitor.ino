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
#include "MonitorIO.h"

// ---------------- 配置存储 ----------------
struct Creds {
  String ssid, wifi, host = "mqtts.heclouds.com", pid, dev, key;
  uint16_t port = onetransport::Port;
  bool ready() const { return !ssid.isEmpty() && !pid.isEmpty() && !dev.isEmpty() && !key.isEmpty(); }
};

static Creds creds;  // Pending edits persisted by save.
static Creds activeCreds;  // Session settings and MQTT host storage, stable until reboot.
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

static bmon::Sample sampleSensors() {
  bmon::Sample s;
  const float t = dht.readTemperature(), h = dht.readHumidity();
  if (bmon::validDht(t, h)) { s.temp = t; s.humi = h; s.dhtOk = true; }
  if (!bmon::kVerify) s.smoke = bmon::readMq(bmon::Mq2Pin);
  s.air = bmon::readMq(bmon::Mq135Pin);
  return s;
}

static bmon::MonitorOutputs outputs;
static StaticSemaphore_t stateStorage;
static SemaphoreHandle_t stateMutex;
static bmon::Sample latest;
static uint32_t sampleAt;
static bool sampled = false, localWorker = false;

// Network operations and serial logging never hold the actuator/snapshot mutex.
static void localCycle() {
  const bmon::Sample s = sampleSensors();
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  const int oldFan = outputs.fan.confirmed, oldServo = outputs.servo.confirmed;
  outputs.automatic(s, millis());
  const int fan = outputs.fan.confirmed, servo = outputs.servo.confirmed;
  latest = s;
  sampleAt = millis();
  sampled = true;
  xSemaphoreGive(stateMutex);
  if (oldFan != fan || oldServo != servo) Serial.printf("[link] fan=%d servo=%d (-1=unknown)\n", fan, servo);
  static unsigned ticks = 0;
  if (++ticks % 5 == 0) Serial.printf("[diag] T=%.1f H=%.1f smoke=%d air=%d fan=%d servo=%d\n", s.temp, s.humi, s.smoke, s.air, fan, servo);
}

static void sampleTask(void*) {
  // Minimum spacing also respects the DHT11 library cache after a late wakeup.
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(2000));
    localCycle();
  }
}

// ---------------- OneNET MQTT ----------------
static onetransport::Client net;
static PubSubClient mqtt(net);
static String topicUp, topicSet, topicReply;
static uint32_t msgSeq = 0;
static uint32_t lastReport = 0;

static void buildTopics() {
  const String base = "$sys/" + activeCreds.pid + "/" + activeCreds.dev;
  topicUp = base + "/thing/property/post";
  topicSet = base + "/thing/property/set";
  topicReply = base + "/thing/property/set_reply";
}

static void report() {
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  const bool fresh = sampled && uint32_t(millis() - sampleAt) < 5000;
  const bmon::Sample s = latest;
  const int fanLevel = outputs.fan.confirmed, servoAngle = outputs.servo.confirmed;
  xSemaphoreGive(stateMutex);
  if (!fresh || (!s.dhtOk && s.smoke < 0 && s.air < 0 && fanLevel < 0 && servoAngle < 0)) return;
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
  if (fanLevel >= 0) p["fan_level"]["value"] = fanLevel;
  if (servoAngle >= 0) p["servo"]["value"] = servoAngle;
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
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  const bool applied = valid && outputs.command(requested.fan, requested.servo, millis());
  xSemaphoreGive(stateMutex);
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
  if (!activeCreds.ready() || WiFi.status() != WL_CONNECTED) { if (mqtt.connected()) mqtt.disconnect(); return; }
  if (mqtt.connected()) { mqtt.loop(); return; }
  uint32_t now = millis();
  if (int32_t(now - nextConnectAt) < 0) return;
  nextConnectAt = now + 5000;
  // OneNET：clientId = 设备名称，username = 产品ID，password = 设备密钥
  String token = onenetMakeToken(activeCreds.pid, activeCreds.dev, activeCreds.key);
  if (token.isEmpty()) { nextConnectAt = now + 3000; return; }
  if (mqtt.connect(activeCreds.dev.c_str(), activeCreds.pid.c_str(), token.c_str())) {
    Serial.println("[mqtt] connected to OneNET");
    mqtt.subscribe(topicSet.c_str(), 1);
  } else {
    Serial.printf("[mqtt] connect FAIL state=%d (state 4 = wrong username/password)\n", mqtt.state());
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n=== B-monitor %s on %s ===\n", bmon::FirmwareVersion, bmon::BoardName);
  loadCreds();
  activeCreds = creds;
  printCreds();

  stateMutex = xSemaphoreCreateMutexStatic(&stateStorage);
  outputs.begin();
  Serial.printf("[init] fan=%d servo=%d (-1=unknown)\n", outputs.fan.confirmed, outputs.servo.confirmed);
  dht.begin();
  analogReadResolution(12);
  if (!bmon::kVerify) analogSetPinAttenuation(bmon::Mq2Pin, ADC_11db);
  if (bmon::Mq135Pin >= 0) analogSetPinAttenuation(bmon::Mq135Pin, ADC_11db);

  localWorker = xTaskCreate(sampleTask, "bmon-local", 4096, nullptr, 2, nullptr) == pdPASS;
  if (!localWorker) { Serial.println("[init] local-only fallback"); return; }
  if (activeCreds.ssid.isEmpty()) {
    Serial.println("no wifi ssid: use cfg ssid/wifi ... then save, then reboot");
    return;
  }
  // Wi-Fi 必须在主任务里启动：放进自建任务会静默失败（2026-11-21 实测）
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(activeCreds.ssid.c_str(), activeCreds.wifi.c_str());
  configTime(0, 0, "ntp.aliyun.com", "cn.pool.ntp.org", "pool.ntp.org");
  buildTopics();
  onetransport::prepare(net);
  mqtt.setServer(activeCreds.host.c_str(), activeCreds.port);
  mqtt.setBufferSize(1024);
  mqtt.setKeepAlive(60);
  mqtt.setCallback(onMessage);

}

void loop() {
  pollSerialConfig();
  if (!localWorker) {
    static uint32_t lastSample = 0;
    if (millis() - lastSample >= 2000) { lastSample = millis(); localCycle(); }
  } else {
    ensureMqtt();
    if (mqtt.connected() && millis() - lastReport >= bmon::ReportIntervalMs) {
      lastReport = millis();
      report();
    }
  }
  delay(20);
}
