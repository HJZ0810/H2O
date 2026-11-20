#include "ExamNetwork.h"
#include "Board.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
bool ExamNetwork::begin(const DeviceConfig& cfg) {
  cfg_=cfg;rx_=xQueueCreate(8,sizeof(NetEvent*));tx_=xQueueCreate(3,sizeof(Tx));sensor_=xQueueCreate(1,sizeof(Tx));
  if(!rx_||!tx_||!sensor_) return false;
  return xTaskCreatePinnedToCore(task,"exam-network",10240,this,1,nullptr,0)==pdPASS;
}
void ExamNetwork::emit(NetKind kind,const char* payload,size_t size) {
  if(!size)size=strlen(payload);
  if(size>8192){overflow_=true;return;}
  NetEvent* e=static_cast<NetEvent*>(malloc(sizeof(NetEvent)+size));
  if(!e){overflow_=true;return;}
  e->kind=kind;e->epoch=epoch_.load();memcpy(e->payload,payload,size);e->payload[size]=0;
  if(xQueueSend(rx_,&e,0)!=pdTRUE){free(e);overflow_=true;}
}
NetEvent* ExamNetwork::poll(){NetEvent* e=nullptr;if(rx_)xQueueReceive(rx_,&e,0);return e;}
bool ExamNetwork::send(const std::string& command,uint32_t observedEpoch) {
  if(!tx_||!online()||observedEpoch!=epoch_.load()||command.empty()||command.size()>=sizeof(Tx::json))return false;
  Tx t{};t.at=millis();t.epoch=observedEpoch;memcpy(t.json,command.c_str(),command.size()+1);
  return xQueueSend(tx_,&t,0)==pdTRUE;
}
void ExamNetwork::telemetry(const String& json) {
  if(!sensor_||!online()||json.length()>=sizeof(Tx::json))return;
  Tx t{};t.at=millis();t.epoch=epoch_.load();memcpy(t.json,json.c_str(),json.length()+1);xQueueOverwrite(sensor_,&t);
}
void ExamNetwork::task(void* self){static_cast<ExamNetwork*>(self)->run();vTaskDelete(nullptr);}
void ExamNetwork::loseLink() {
  if(!online_.exchange(false))return;
  epoch_.fetch_add(1);emit(NetKind::Offline);
  Tx t{};while(xQueueReceive(tx_,&t,0)==pdTRUE)emit(NetKind::Error,"断线前未发送的请求已取消，未重试");
  xQueueReset(sensor_);
}
void ExamNetwork::run() {
  WiFiClient tcp;tcp.setConnectionTimeout(2000);tcp.setTimeout(2000);
  PubSubClient mqtt(tcp);mqtt.setServer(cfg_.host.c_str(),cfg_.port);
  mqtt.setKeepAlive(30);mqtt.setSocketTimeout(2);
  if(!mqtt.setBufferSize(8320)){emit(NetKind::Error,"MQTT缓冲区分配失败");return;}
  String up="/iot/game/"+cfg_.group+"/up",down="/iot/game/"+cfg_.group+"/down";
  String clientId="zlc-"+cfg_.group+"-"+WiFi.macAddress();clientId.replace(":","");
  mqtt.setCallback([this,&down](char* topic,uint8_t* payload,unsigned int length){
    if(down==topic)emit(NetKind::Message,reinterpret_cast<const char*>(payload),length);
  });
  uint32_t nextConnect=millis(),helloAt=0;
  auto hello=[&]{JsonDocument d;d["type"]="hello";d["data"]["name"]=cfg_.name;
    d["data"]["firmware"]=board::FirmwareVersion;d["data"]["protocol"]=2;String s;serializeJson(d,s);
    return mqtt.publish(up.c_str(),s.c_str(),false);};
  for(;;) {
    uint32_t now=millis();
    if(WiFi.status()!=WL_CONNECTED) {
      loseLink();
      if(mqtt.connected())mqtt.disconnect();
    } else if(!mqtt.connected()) {
      loseLink();
      if(int32_t(now-nextConnect)>=0) {
        nextConnect=now+5000;
        if(mqtt.connect(clientId.c_str(),cfg_.group.c_str(),cfg_.mqttPassword.c_str())) {
          if(mqtt.subscribe(down.c_str(),1)) {
            online_=true;emit(NetKind::Online);hello();helloAt=now;
          } else {mqtt.disconnect();emit(NetKind::Error,"MQTT订阅发送失败");}
        } else {
          String err="MQTT连接失败，状态码 "+String(mqtt.state())+"；检查组号、密码及网络";
          emit(NetKind::Error,err.c_str());
        }
      }
    } else {
      if(!mqtt.loop()){loseLink();}
      else {
        Tx t{};
        while(xQueueReceive(tx_,&t,0)==pdTRUE) {
          // Send once only. Expired or failed score-changing messages are never retried.
          if(t.epoch!=epoch_.load() || uint32_t(millis()-t.at)>5000 || !mqtt.connected() || !mqtt.publish(up.c_str(),t.json,false)) emit(NetKind::Error,"考试请求已过期或连接已改变，未发送重试");
        }
        if(xQueueReceive(sensor_,&t,0)==pdTRUE && t.epoch==epoch_.load() && uint32_t(millis()-t.at)<10000) mqtt.publish(up.c_str(),t.json,false);
        if(uint32_t(now-helloAt)>=60000) {helloAt=now;hello();}
      }
    }
    if(!online()) {Tx t{};while(xQueueReceive(tx_,&t,0)==pdTRUE)emit(NetKind::Error,"请求发送前已断线，未重试");xQueueReset(sensor_);}
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
