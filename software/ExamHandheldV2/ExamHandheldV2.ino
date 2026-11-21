// R9 手柄 V2：1.8" 128x160 横屏高对比界面。
// 业务层沿用 v1.4 的题库、云端与执行器实现，显示层换成 Panel + Canvas + UiRender。
#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include "Board.h"
#include "Panel.h"
#include "Canvas.h"
#include "Theme.h"
#include "UiRender.h"
#include "FontCN.h"
#include "Actuators.h"
#include "AutoDemo.h"
#include "ButtonLogic.h"
#include "Config.h"
#include "OneNetCfg.h"
#include "Mirror.h"
#include "ExamCore.h"
#include "ExamNetwork.h"
#include "Auth.h"
#include "Sensors.h"

static dev::Panel panel;
static dev::FontCN font;
static ui::Theme theme;
static ui::Canvas canvas;
static ui::Painter* painter = nullptr;
static uint16_t* framebuf = nullptr;

Configuration configuration;
exam::Core session;
ExamNetwork network;
Sensors sensors;
Actuators fan;
OneNet onenet;
Mirror mirror;
bool onenetActive = false;   // 菜单第6项切换：开启后按10秒周期上报并跑联动
exam::AutoDemo demo;
ButtonLogic buttons[board::KeyCount];

enum class View { Menu, Question, Story, Judge, Finished, Recovery, Sensor, Info, Setup, Fan, Monitor };
View view = View::Menu;
int menuIndex = 0;
unsigned optionCursor = 0;
int qScroll = 0;
String feedback, lastKey;
uint32_t feedbackAt = 0, lastReport = 0, lastPaint = 0, linkEpoch = 0, lastResume = 0;
bool networkStarted = false;
unsigned paintedRevision = 0;

static void tell(const String& s) { feedback = s; feedbackAt = millis(); }

static void sendCommand(const std::string& cmd) {
  if (!cmd.empty() && !network.send(cmd, linkEpoch))
    session.fail("请求未发送，等待恢复连接并同步原进度。");
}
static bool examActive() {
  return session.phase == exam::Phase::Question || session.phase == exam::Phase::Story ||
         session.phase == exam::Phase::Judge;
}
static void syncLink() {
  const auto before = network.epoch();
  const bool up = network.online();
  const auto after = network.epoch();
  if (before != after) return;
  if (up) {
    if (!session.connected || linkEpoch != after) {
      if (session.connected) session.link(false);
      linkEpoch = after;
      session.link(true);
    }
  } else if (session.connected) {
    session.link(false);
  }
}

static ui::Status makeStatus() {
  ui::Status s;
  s.group = configuration.value.group.c_str();
  s.battery = -1;
  s.rssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
  s.online = session.ready && session.connected;
  s.score = session.score;
  s.scoreKnown = session.statusKnown;
  s.clock = nullptr;
  return s;
}

static void render() {
  if (!painter) return;
  painter->background();

  const auto& f = session.frame;
  // 以 phase 为准渲染题目：开始考试后 view 仍是 Menu，若要求 view==Question 会永远进不去。
  if (session.phase == exam::Phase::Question && view == View::Question) {
    static ui::OptionView opts[8];
    int n = int(f.options.size());
    if (n > 8) n = 8;
    for (int i = 0; i < n; i++) {
      opts[i].key = f.options[i].key.c_str();
      opts[i].text = f.options[i].text.c_str();
      opts[i].selected = (session.selected >> i) & 1U;
      opts[i].removed = (session.removed >> i) & 1U;
    }
    ui::QuestionView q;
    q.level = f.level;
    q.typeName = f.questionType.c_str();
    q.text = f.text.c_str();
    q.options = opts;
    q.optionCount = n;
    q.cursor = int(optionCursor);
    q.seq = f.seq;
    q.total = f.total;
    painter->question(q, qScroll);
    painter->statusBar(makeStatus());
  } else {
    switch (view) {
      case View::Monitor:
      case View::Sensor: {
        static char mq0v[16], mq1v[16];
        snprintf(mq0v, sizeof(mq0v), "%s", sensors.mq[0].enabled ? String(sensors.mq[0].raw).c_str() : "--");
        snprintf(mq1v, sizeof(mq1v), "%s", sensors.mq[1].enabled ? String(sensors.mq[1].raw).c_str() : "--");
        ui::Metric mq[2];
        mq[0] = {"MQ-2", mq0v, "ADC", theme.ok};
        mq[1] = {"MQ-135", mq1v, "ADC", theme.warn};
        painter->sensorPage(sensors.dhtValid, sensors.temperature, sensors.humidity,
                            mq, 2, !board::kHasMq);
        painter->statusBar(makeStatus());
        painter->pageTitle("环境采样");
        break;
      }
      case View::Info: {
        String ip = WiFi.localIP().toString();
        String rssi = String(WiFi.RSSI()) + " dBm";
        const char* keys[] = {"固件", "组号", "Wi-Fi", "IP", "信号", "MQTT"};
        const char* vals[] = {board::FirmwareVersion, configuration.value.group.c_str(),
                              configuration.value.ssid.c_str(), ip.c_str(), rssi.c_str(),
                              session.ready ? "已连接" : "等待"};
        painter->infoPage(keys, vals, 6);
        painter->statusBar(makeStatus());
        painter->pageTitle("设备信息");
        break;
      }
      case View::Setup: {
        painter->setupPage(configuration.apName.c_str(), configuration.apPassword.c_str(),
                           "192.168.4.1", WiFi.macAddress().c_str());
        painter->statusBar(makeStatus());
        painter->pageTitle("设备配网");
        break;
      }
      case View::Fan: {
        painter->fanPage(fan.ready(), fan.channelB(), fan.speedPercent(false),
                         fan.speedPercent(true), fan.pulseWidth(), fan.pulseWidth() > 0);
        painter->statusBar(makeStatus());
        painter->pageTitle("执行器");
        break;
      }
      default: {
        auto& fr = session.frame;
        if (view == View::Judge || session.phase == exam::Phase::Judge) {
          painter->judge(fr.correct, fr.answer.c_str(), fr.gain, session.hp, session.score, fr.text.c_str());
          painter->statusBar(makeStatus());
          painter->pageTitle("判题");
        } else if (session.phase == exam::Phase::Recovery) {
          painter->recoveryPage(session.notice.c_str(), network.online());
          painter->statusBar(makeStatus());
          painter->pageTitle("同步");
        } else if (session.phase == exam::Phase::Finished) {
          painter->finishedPage(session.score, fr.rank, fr.certificate.c_str());
          painter->statusBar(makeStatus());
          painter->pageTitle("已通关");
        } else if (session.phase == exam::Phase::Story) {
          painter->story(fr.level, fr.text.c_str(), fr.seq, fr.total);
          painter->statusBar(makeStatus());
          painter->pageTitle("剧情");
        } else {
          ui::MenuEntry items[6] = {
              {"开始考试", nullptr, 0}, {"环境采样", nullptr, 1}, {"设备信息", nullptr, 2},
              {"配网设置", nullptr, 3}, {"执行器", nullptr, 4}, {"云端监测", nullptr, 5}};
          if (demo.active()) items[menuIndex].label = "演示中…";
          painter->menu(items, 6, menuIndex);
          painter->statusBar(makeStatus());
          painter->pageTitle("智联迷城", session.ready ? (session.statusKnown ? "ON" : "SYNC") : "OFF",
                             session.ready ? theme.ok : theme.danger);
        }
      }
    }
    // 底栏提示按当前阶段给出，避免把「上下选择」写到只认 OK 的页面
    const char* hint = "上下选 OK进入";
    if (session.phase == exam::Phase::Story) hint = "OK 继续";
    else if (session.phase == exam::Phase::Judge) hint = "OK 继续";
    else if (session.phase == exam::Phase::Question) hint = "左右选 F1勾 OK交";
    else if (session.phase == exam::Phase::Finished) hint = "长按返回采样";
    else if (session.phase == exam::Phase::Recovery) hint = "OK 重新同步";
    if (view == View::Sensor || view == View::Info || view == View::Setup || view == View::Fan ||
        view == View::Monitor)
      hint = "OK/返回 退出";
    painter->footerBar(hint, "");
  }

  if (!feedback.isEmpty() && uint32_t(millis() - feedbackAt) < 2500) painter->toast(feedback.c_str());
  panel.push(0, 0, panel.width(), panel.height(), framebuf);
  paintedRevision = session.revision;
}

static void switchView(View v) {
  if (view == View::Fan && v != View::Fan) fan.stop();
  view = v;
  qScroll = 0;
  lastKey = "";
}

static void handleKey(board::Key key, Press press) {
  using namespace board;
  if (press == Press::None) return;
  Serial.print("[key] k=");
  Serial.print(int(key));
  Serial.print(" p=");
  Serial.print(int(press));
  Serial.print(" phase=");
  Serial.print(int(session.phase));
  Serial.print(" view=");
  Serial.print(int(view));
  Serial.print(" menu=");
  Serial.print(menuIndex);
  Serial.print(" rev=");
  Serial.print(session.revision);
  Serial.print("/");
  Serial.println(paintedRevision);
  if (demo.active()) {
    demo.stop();
    tell("已停止演示，可手动作答");
    return;
  }
  if (view == View::Sensor || view == View::Info) {
    if (key == Confirm || key == Back) switchView(View::Menu);
    return;
  }
  if (view == View::Setup) {
    if (key == Back) switchView(View::Menu);
    return;
  }
  if (view == View::Fan) {
    if (key == Back) switchView(View::Menu);
    else if (key == Confirm) fan.stop();
    else if (key == Test) fan.selectNextChannel();
    else if (key == Up) fan.changeSpeed(1);
    else if (key == Down) fan.changeSpeed(-1);
    else if (key == Left) fan.turn(-1);
    else if (key == Right) fan.turn(1);
    return;
  }
  if (press == Press::Long) {
    if (key == Test && view == View::Menu) {
      if (demo.start(session, millis())) switchView(View::Menu);
      else tell("需先同步进度");
    } else if (key == Confirm && session.phase == exam::Phase::Question) {
      auto cmd = session.hint(millis());
      if (cmd.empty()) tell("暂无锦囊");
      sendCommand(cmd);
    } else if (key == Back && view == View::Menu) {
      switchView(View::Sensor);
    }
    return;
  }
  if (view == View::Menu) {  // 以界面为准：画的是菜单就必须响应菜单按键
    if (key == Up) menuIndex = (menuIndex + 5) % 6;
    else if (key == Down) menuIndex = (menuIndex + 1) % 6;
    else if (key == Test) {
      if (!demo.start(session, millis())) tell("需先同步进度");
    } else if (key == Confirm) {
      const View views[] = {View::Menu, View::Sensor, View::Info, View::Setup, View::Fan, View::Monitor};
      if (menuIndex == 0) {
        if (!session.ready || !session.statusKnown) { tell("连接未就绪"); return; }
        sendCommand(session.start(millis()));
        switchView(View::Question);
      } else if (menuIndex == 5) {
        if (!onenet.configured()) { tell("未配置 OneNET"); return; }
        onenetActive = !onenetActive;
        tell(onenetActive ? "云端监测已开" : "云端监测已关");
        switchView(View::Monitor);
      } else {
        switchView(views[menuIndex]);
      }
    }
    return;
  }
  if (session.phase == exam::Phase::Question) {
    const unsigned n = session.frame.options.size();
    if (key == Left || key == Right) {
      if (n) optionCursor = (optionCursor + (key == Left ? n - 1 : 1)) % n;
    } else if (key == Up) {
      qScroll -= theme.lineStep;
      if (qScroll < 0) qScroll = 0;
    } else if (key == Down) {
      qScroll += theme.lineStep;
    } else if (key == Test) {
      if (!session.choose(optionCursor)) tell("该选项不可选");
    } else if (key == Confirm) {
      auto cmd = session.submit(millis());
      if (cmd.empty()) tell("请先选择");
      sendCommand(cmd);
    }
    return;
  }
  if (key == Confirm) {
    if (paintedRevision != session.revision) return;
    sendCommand(session.advance(millis()));
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("[boot] serial ok");
  for (auto pin : board::Keys) if (pin >= 0) pinMode(pin, INPUT_PULLUP);
  Serial.println("[boot] keys ok");

  framebuf = static_cast<uint16_t*>(ps_malloc(size_t(board::TFT_DEFAULT_W) * board::TFT_DEFAULT_H * 2));
  Serial.print("[boot] fb=");
  Serial.println((unsigned long)framebuf, 16);
  if (!framebuf) {
    Serial.println("[boot] framebuffer alloc failed");
    return;
  }
  panel.begin(board::TFT_SCK, board::TFT_MOSI, board::TFT_CS, board::TFT_DC,
              board::TFT_RST, board::TFT_BL, board::TFT_CTRL,
              board::TFT_DEFAULT_W, board::TFT_DEFAULT_H, board::TFT_DEFAULT_ROT);
  Serial.print("[boot] panel ");
  Serial.print(panel.width());
  Serial.print("x");
  Serial.println(panel.height());
  theme.layout(panel.width(), panel.height());
  Serial.println("[boot] theme ok");
  canvas.attach(framebuf, panel.width(), panel.height());
  font.attach(&canvas);
  font.useSize(theme.bodyPx);
  Serial.println("[boot] font ok");
  painter = new ui::Painter(canvas, theme, font);
  Serial.println("[boot] painter ok");

  fan.begin();
  configuration.load();
  // 接入密码由固件按“组号 + MAC”算出（任务 A 硬性要求，不得手填）。
  // 放在 usable() 判断之前，组号有效时即使配置页没填密码也能直接联网。
  {
    String derived = auth::derivePassword(configuration.value.group, auth::deviceMac());
    if (!derived.isEmpty()) {
      configuration.value.mqttPassword = derived;
      Serial.print("[auth] group=");
      Serial.print(configuration.value.group);
      Serial.print(" mac=");
      Serial.print(auth::deviceMac());
      Serial.print(" pass=");
      Serial.println(derived);
    } else {
      Serial.println("[auth] group not numeric, using stored password");
    }
  }
  sensors.begin(configuration.value);
  onenetcfg::loadFromNvs(onenet, onenetActive, fan);
  mirror.configure("yhyegj-01", "186.241.90.115", 1884, "h2o", "H2oDemo2026");
  mirror.attach(&fan);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  configTime(0, 0, "ntp.aliyun.com", "cn.pool.ntp.org");

  char prefix[25];
  snprintf(prefix, sizeof(prefix), "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
  session.setRequestPrefix(prefix);

  Serial.printf("R9 v2 firmware %s on %s; %dx%d\n", board::FirmwareVersion, board::BoardName,
                panel.width(), panel.height());

  Serial.println("[boot] first render");
  render();
  Serial.println("[boot] render ok");
  if (!configuration.value.usable()) {
    configuration.openPortal();
    switchView(View::Setup);
  } else {
    networkStarted = network.begin(configuration.value);
    if (!networkStarted) session.fail("网络任务创建失败");
  }
  render();
}

#include "UiDebug.h"

void loop() {
  pollUiDebug();
  configuration.tick();
  sensors.tick();
  if (onenetActive) onenet.tick(sensors, fan);
  mirror.tick(sensors, fan, onenet.fanLevel(), onenet.servoAngle());

  if (networkStarted) {
    if (network.overflowed()) session.fail("接收队列溢出，已暂停");
    syncLink();
    while (auto e = network.poll()) {
      syncLink();
      if (e->kind == NetKind::Message && e->epoch == linkEpoch && session.connected) session.receive(e->payload);
      else if (e->kind == NetKind::Error && e->epoch == linkEpoch) {
        if (examActive() || session.busy()) session.fail(e->payload);
        else tell(e->payload);
      }
      free(e);
    }
  }

  session.tick(millis());
  if (session.needsResume() && uint32_t(millis() - lastResume) >= 2000) {
    lastResume = millis();
    sendCommand(session.resume(millis()));
  }
  if (lastKey != session.frame.qid.c_str()) {
    lastKey = session.frame.qid.c_str();
    optionCursor = 0;
    qScroll = 0;
  }

  for (unsigned i = 0; i < board::KeyCount; i++) {
    if (board::Keys[i] < 0) continue;
    auto press = buttons[i].update(digitalRead(board::Keys[i]) == LOW, millis());
    handleKey(static_cast<board::Key>(i), press);
  }

  if (demo.active()) {
    auto step = demo.tick(session, millis(), true, view == View::Menu && paintedRevision == session.revision);
    sendCommand(step.command);
  }

  if (network.online() && uint32_t(millis() - lastReport) >= 5000) {
    lastReport = millis();
    network.telemetry(sensors.json());
  }
  static uint32_t lastDiag = 0;
  if (uint32_t(millis() - lastDiag) >= 3000) {
    lastDiag = millis();
    String ip = WiFi.localIP().toString();
    Serial.print("[net] wifi=");
    Serial.print(int(WiFi.status()));
    Serial.print(" ip=");
    Serial.print(ip);
    Serial.print(" rssi=");
    Serial.print(WiFi.RSSI());
    Serial.print(" online=");
    Serial.print(network.online() ? 1 : 0);
    Serial.print(" ready=");
    Serial.print(session.ready ? 1 : 0);
    Serial.print(" grp=");
    Serial.print(configuration.value.group);
    Serial.print(" phase=");
    Serial.print(int(session.phase));
    Serial.print(" busy=");
    Serial.print(session.busy() ? 1 : 0);
    Serial.print(" notice=");
    Serial.println(session.notice.c_str());
  }

  if (uint32_t(millis() - lastPaint) >= 80) {
    lastPaint = millis();
    render();
  }
  delay(2);
}
