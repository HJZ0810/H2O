#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include "Board.h"
#include "Actuators.h"
#include "ButtonLogic.h"
#include "Config.h"
#include "Display.h"
#include "ExamCore.h"
#include "AutoDemo.h"
#include "ExamNetwork.h"
#include "Sensors.h"

Configuration configuration;
Display screen;
exam::Core session;
exam::AutoDemo demo;
ExamNetwork network;
Sensors sensors;
Actuators fan;
ButtonLogic buttons[board::KeyCount];
enum class View {Main,Sensor,Info,ConfirmStart,ConfirmSetup,Setup,Fan};
View view=View::Main,returnView=View::Main;
constexpr int menuCount=5;
int menuIndex=0;
unsigned optionCursor=0;
std::string cursorQuestion;
String feedback,lastScreenKey;
uint32_t feedbackAt=0,lastReport=0,lastPaint=0,linkEpoch=0,lastResume=0;
bool networkStarted=false;
unsigned paintedRevision=0;
bool swallowKey[board::KeyCount]={};

void tell(const String& text){feedback=text;feedbackAt=millis();}
void sendCommand(const std::string& command) {
  if(!command.empty()&&!network.send(command,linkEpoch))session.fail("请求未发送，等待恢复连接并同步原进度。");
}
void switchView(View v) {
  if (view==View::Fan && v!=View::Fan) fan.stop();
  view=v;
  lastScreenKey="";
}
void startDemo(){if(demo.start(session,millis())){feedback="";switchView(View::Main);}else tell("先等待进度同步；已通关组不能重置成绩");}
bool examActive(){return session.phase==exam::Phase::Question||session.phase==exam::Phase::Story||session.phase==exam::Phase::Judge;}
void syncLinkState(){
  // Connection events can be dropped when the receive queue is full. Read the
  // network task's authoritative atomics instead of relying on queued edges.
  const auto before=network.epoch();const bool up=network.online();const auto after=network.epoch();
  if(before!=after)return;
  if(up){if(!session.connected||linkEpoch!=after){if(session.connected)session.link(false);linkEpoch=after;session.link(true);}}
  else if(session.connected)session.link(false);
}

String sensorBody() {
  String s=sensors.dhtValid?"温度 "+String(sensors.temperature,1)+" C\n湿度 "+String(sensors.humidity,1)+" %":"DHT11: 无有效读数\n检查接口与3.3V模块";
  for(int i=0;i<2;i++) {
    s+=i==0?"\nMQ-2 ":"\nMQ-135 ";
    if(!sensors.mq[i].enabled)s+="未启用";
    else {s+=String(sensors.mq[i].raw)+" ADC\nAO "+String(sensors.mq[i].mv/board::MqDivider/1000.0f,3)+" V";if(sensors.mq[i].clipped)s+=" [超量程]";}
  }
  return s+"\nMQ未经标定，无ppm\n数据每5秒上报一次";
}
void render() {
  String title,body,foot="上下读  左右翻页",badge;
  String key=String(int(view))+"/"+int(session.phase)+"/"+session.frame.type.c_str()+"/"+session.frame.qid.c_str()+"/"+session.frame.level+"/"+session.frame.seq;
  bool reset=key!=lastScreenKey;if(reset)lastScreenKey=key;
  if(view==View::Setup) {
    title="设备配网";
    body="手机连接热点：\n"+configuration.apName+"\n热点密码：\n"+configuration.apPassword+"\n浏览器打开\nhttp://192.168.4.1\nMAC: "+WiFi.macAddress()+"\n教师先注册本组账号";
    if(!configuration.message.isEmpty())body+="\n"+configuration.message;
    foot="上下滚动查看";
  } else if(view==View::Fan) {
    title="风扇控制";
    body=String(fan.channelB() ? "选择 B" : "选择 A")+"  A "+String(fan.speedPercent(false))+
         "% / B "+String(fan.speedPercent(true))+"%\n"+
         "手柄输出就绪"+
         (fan.pulseWidth() ? "\n舵机 "+String(fan.pulseWidth())+" us" : "\n舵机待命")+
         "\n测试切换 A/B，上下调速\n左右转向，确认停止";
    if(!fan.ready())body="PWM 未就绪，输出保持关闭。";
    foot="上下调速 左右转向";
  } else if(view==View::ConfirmStart) {
    title="开始 / 继续考试";
    body="首次开始将进入第一关。\n已有进度会继续原题。\n每题最多计分一次，总分350分。\n断线后自动同步原进度。\n\nOK: 开始 / 继续\n左键: 返回";
    foot="OK确认  左键取消";
  } else if(view==View::ConfirmSetup) {
    title="打开配网设置？";body="配网保存会重启设备。\n相同组号重连后恢复服务器进度。\n建议在考试前设置。\n\nOK: 打开热点\n左键: 取消";foot="OK确认  左键取消";
  } else if(view==View::Sensor) {title="环境采样";body=sensorBody();foot="OK返回  上下滚动";}
  else if(view==View::Info) {
    title="设备信息";body="固件 "+String(board::FirmwareVersion)+"\n组号 "+configuration.value.group+"\nWi-Fi MAC\n"+WiFi.macAddress()+"\nIP "+WiFi.localIP().toString()+"\nWi-Fi "+String(WiFi.status()==WL_CONNECTED?"已连接":"等待连接")+"\nMQTT "+String(session.ready?"就绪":"等待连接/握手")+"\n进度同步 "+String(session.statusKnown?"完成":"等待MQTT快照")+"\n屏幕 160x128 横屏\nMQ DO: 未接主控\n电脑烧录前先拔外接5V\n长按F2进入配网确认";foot="OK返回  长按F2设置";
  } else {
    badge=session.ready?(session.statusKnown?"在线":"同步"):"离线";
    auto& f=session.frame;
    if(session.phase==exam::Phase::Menu) {
      title="智联迷城";body="组号 "+configuration.value.group+"\n"+(session.statusKnown?"累计 "+String(session.score)+" 分":"等待同步累计分数")+"\n";
      const char* entries[]={"开始 / 继续考试","环境采样","设备信息 / MAC","配网与模块设置","风扇控制"};
      for(int i=0;i<menuCount;i++)body+=String(menuIndex==i?"> ":"  ")+entries[i]+"\n";
      foot="F1演示  上下选 OK进入";
    } else if(session.phase==exam::Phase::Recovery) {
      title="正在恢复进度";body=String(session.notice.c_str())+"\n\n连接恢复后自动同步原题或解析。\nOK: 再次同步\nF1: 设备信息\n长按返回: 环境采样";foot="OK同步  长按返回采样";
    } else if(session.phase==exam::Phase::Story) {
      title="第"+String(f.level)+"关 · 剧情";body=String(f.text.c_str())+"\n\n[剧情 "+f.seq+"/"+f.total+"]";foot="OK继续  上下阅读";
    } else if(session.phase==exam::Phase::Question) {
      title="第"+String(f.level)+"关 "+String(f.questionType=="multi"?"多选":f.questionType=="bool"?"判断":"单选");
      body=String(f.text.c_str())+"\n";
      for(unsigned i=0;i<f.options.size();i++) {
        body+="\n"+String(i==optionCursor?">":" ")+String((session.removed&(1U<<i))?"[-] ":(session.selected&(1U<<i))?"[x] ":"[ ] ")+f.options[i].key.c_str()+" "+f.options[i].text.c_str();
      }
      body+="\n\n左右定位 A-D，测试键勾选。\nOK提交，长按OK锦囊。";
      String choice=optionCursor<f.options.size()?f.options[optionCursor].key.c_str():"-";
      foot="左右 "+choice+"  测试勾选 OK交";
    } else if(session.phase==exam::Phase::Judge) {
      title=f.correct?"回答正确":"判题解析";body="正确答案 "+String(f.answer.c_str())+"\n本题 +"+f.gain+" 分 / 剩余生命 "+session.hp+"\n累计 "+session.score+" 分\n\n"+f.text.c_str();foot="OK读完继续  上下阅读";
    } else {
      title="智联迷城 · 已通关";body=String(f.text.c_str())+"\n\n总分 "+session.score+"\n排名 "+f.rank+"\n证书\n"+f.certificate.c_str()+"\n\n学生端查看本组结果\n长按返回: 环境采样";foot="长按返回采样  F1信息";
    }
    if(session.busy())foot="等待服务器，请勿重复按";
    else if(demo.active())foot=demo.status().c_str();
    if(!feedback.isEmpty()&&uint32_t(millis()-feedbackAt)<3500)body+="\n\n"+feedback;
  }
  screen.show(title,body,foot,badge,reset);
  paintedRevision=session.revision;
}
void handleKey(board::Key key, Press press) {
  using namespace board;
  if (press==Press::None) return;
  if (demo.active()) {
    demo.stop();
    tell("自动演示已停止，可手动作答");
    return;
  }
  if (view==View::Fan) {
    if (key==Back) switchView(View::Main);
    else if (key==Confirm) fan.stop();
    else if (press==Press::Short && key==Test) fan.selectNextChannel();
    else if (press==Press::Short && key==Up) fan.changeSpeed(1);
    else if (press==Press::Short && key==Down) fan.changeSpeed(-1);
    else if (press==Press::Short && key==Left) fan.turn(-1);
    else if (press==Press::Short && key==Right) fan.turn(1);
    return;
  }
  if (press==Press::Long) {
    if (key==Test && view==View::Main) startDemo();
    else if (key==Confirm && view==View::Main && session.phase==exam::Phase::Question) {
      auto command=session.hint(millis());
      if(command.empty())tell("暂无锦囊或正在等待回执");
      sendCommand(command);
    } else if (key==Back && view==View::Main) {
      returnView=view;
      switchView(View::Sensor);
    } else if (key==Back && view==View::Info) switchView(View::ConfirmSetup);
    return;
  }
  if (view==View::Sensor || view==View::Info) {
    if (key==Confirm || key==Back) switchView(returnView);
    else if (key==Up) screen.scroll(-1);
    else if (key==Down) screen.scroll(1);
    else if (key==Left) screen.scroll(-6);
    else if (key==Right) screen.nextPage();
    return;
  }
  if (view==View::ConfirmStart || view==View::ConfirmSetup) {
    if (key==Left || key==Back) switchView(View::Main);
    else if (key==Up) screen.scroll(-1);
    else if (key==Down) screen.scroll(1);
    else if (key==Confirm) {
      if (lastScreenKey.isEmpty()) return;
      if (!screen.lastPage()) { screen.nextPage(); return; }
      if (view==View::ConfirmStart) {
        if (!session.ready || !session.statusKnown) tell("连接或成绩同步未就绪，请稍候");
        else sendCommand(session.start(millis()));
        switchView(View::Main);
      } else {
        configuration.openPortal();
        switchView(View::Setup);
      }
    }
    return;
  }
  if (view==View::Setup) {
    if (key==Up) screen.scroll(-1);
    else if (key==Down) screen.scroll(1);
    return;
  }
  if (session.phase==exam::Phase::Menu) {
    if (key==Test) startDemo();
    else if (key==Up) menuIndex=(menuIndex+menuCount-1)%menuCount;
    else if (key==Down) menuIndex=(menuIndex+1)%menuCount;
    else if (key==Confirm) {
      returnView=view;
      const View views[]={View::ConfirmStart,View::Sensor,View::Info,View::ConfirmSetup,View::Fan};
      switchView(views[menuIndex]);
    }
    return;
  }
  if (session.phase==exam::Phase::Recovery || session.phase==exam::Phase::Finished) {
    if (key==Test) { returnView=view; switchView(View::Info); }
    else if (key==Back) { returnView=view; switchView(View::Sensor); }
    else if (key==Confirm && session.phase==exam::Phase::Recovery) session.fail("正在请求服务器原进度");
    else if (key==Up) screen.scroll(-1);
    else if (key==Down) screen.scroll(1);
    return;
  }
  if (session.busy()) return;
  if (key==Up) { screen.scroll(-1); return; }
  if (key==Down) { screen.scroll(1); return; }
  if (session.phase==exam::Phase::Question) {
    const unsigned count=session.frame.options.size();
    if (key==Left || key==Right) {
      if (count) optionCursor=(optionCursor+(key==Left?count-1:1))%count;
    } else if (key==Test) {
      if (!session.choose(optionCursor)) tell("该选项不可选");
    } else if (key==Confirm) {
      auto command=session.submit(millis());
      if(command.empty())tell("请先选择答案");
      sendCommand(command);
    }
  } else {
    if (key==Left) screen.scroll(-6);
    else if (key==Right) screen.nextPage();
    else if (key==Confirm) {
      if (lastScreenKey.isEmpty() || paintedRevision!=session.revision) return;
      if (!screen.lastPage()) screen.nextPage();
      else sendCommand(session.advance(millis()));
    }
  }
}
void setup() {
  fan.begin();
  Serial.begin(115200);for(auto pin:board::Keys)pinMode(pin,INPUT_PULLUP);
  char requestPrefix[25];snprintf(requestPrefix,sizeof(requestPrefix),"%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random());session.setRequestPrefix(requestPrefix);
  delay(35);bool forceSetup=digitalRead(board::Keys[board::Test])==LOW;
  WiFi.mode(WIFI_STA);WiFi.setSleep(false);configuration.load();screen.begin(configuration.value);sensors.begin(configuration.value);
  Serial.printf("ZLC firmware %s; STA MAC %s; GPIO matched to R9\n",board::FirmwareVersion,WiFi.macAddress().c_str());
  if(forceSetup||!configuration.value.usable()){configuration.openPortal();switchView(View::Setup);}
  else {networkStarted=network.begin(configuration.value);if(!networkStarted)session.fail("网络任务或队列创建失败，请重启");}
  render();
}
void loop() {
  configuration.tick();sensors.tick();
  if(networkStarted){
    if(network.overflowed())session.fail("接收队列或内存不足，已暂停，未丢弃后继续答题");
    syncLinkState();
    while(auto e=network.poll()){
      syncLinkState();
      if(e->kind==NetKind::Message && e->epoch==linkEpoch && session.connected)session.receive(e->payload);
      else if(e->kind==NetKind::Error&&e->epoch==linkEpoch){if(examActive()||session.busy())session.fail(e->payload);else tell(e->payload);}
      free(e);
    }
  }
  session.tick(millis());
  if(session.needsResume()&&uint32_t(millis()-lastResume)>=2000){lastResume=millis();sendCommand(session.resume(millis()));}
  if (cursorQuestion!=session.frame.qid) {
    cursorQuestion=session.frame.qid;
    optionCursor=0;
  }
  for (unsigned i=0;i<board::KeyCount;i++){
    auto press=buttons[i].update(digitalRead(board::Keys[i])==LOW,millis());
    bool downEdge=buttons[i].takeDownEdge();
    // Stop on the debounced press, before demo.tick can send another request.
    // Consume its later release/long event so it cannot also choose an answer.
    if(demo.active()&&downEdge){demo.stop();tell("自动演示已停止，可手动作答");swallowKey[i]=true;}
    if(swallowKey[i]){if(!buttons[i].down())swallowKey[i]=false;continue;}
    handleKey(static_cast<board::Key>(i),press);
  }
  if(demo.active()){
    auto step=demo.tick(session,millis(),screen.lastPage(),view==View::Main&&paintedRevision==session.revision&&!lastScreenKey.isEmpty());
    if(step.action==exam::DemoAction::NextPage)screen.nextPage();
    else if(step.action==exam::DemoAction::FirstPage)screen.scroll(-10000);
    sendCommand(step.command);
    if(!demo.active())tell(demo.status().c_str());
  }
  if(network.online()&&uint32_t(millis()-lastReport)>=5000){lastReport=millis();network.telemetry(sensors.json());}
  if(uint32_t(millis()-lastPaint)>=100){lastPaint=millis();render();}
  delay(3);
}
