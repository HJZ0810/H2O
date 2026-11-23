#include "Config.h"
#include "PortalHtml.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_system.h>
void Configuration::load() {
  Preferences p;p.begin("zlc-config",true);
  value.ssid=p.getString("ssid","");value.wifiPassword=p.getString("wifi","");
  value.group=p.getString("group","");value.mqttPassword=p.getString("mqpass","");
  value.name=p.getString("name","手柄");value.host=p.getString("host","124.70.71.221");
  value.port=p.getUShort("port",1883);value.rotation=p.getUChar("rotation",1);value.panel=p.getUChar("panel",0);
  value.mq1=p.getBool("mq1",true);value.mq2=p.getBool("mq2",true);
  JsonDocument cloud;
  if (!deserializeJson(cloud,p.getString("thingscloud",""))) {
    value.cloud.host=cloud["host"]|"";
    value.cloud.accessToken=cloud["token"]|"";
    value.cloud.projectKey=cloud["key"]|"";
  }
  p.end();
}
bool Configuration::save(const DeviceConfig& next) {
  Preferences p;
  if (!p.begin("zlc-config",false)) return false;
  JsonDocument cloud;
  cloud["host"]=next.cloud.host;
  cloud["token"]=next.cloud.accessToken;
  cloud["key"]=next.cloud.projectKey;
  String data;
  serializeJson(cloud,data);
  if (p.putString("thingscloud",data)!=data.length()) return false;
  // Remove any unused profile left by older firmware on the next valid save.
  if (p.isKey("demo_answers") && !p.remove("demo_answers")) return false;
  return p.putString("ssid",next.ssid)==next.ssid.length() &&
         p.putString("wifi",next.wifiPassword)==next.wifiPassword.length() &&
         p.putString("group",next.group)==next.group.length() &&
         p.putString("mqpass",next.mqttPassword)==next.mqttPassword.length() &&
         p.putString("name",next.name)==next.name.length() &&
         p.putString("host",next.host)==next.host.length() &&
         p.putUShort("port",next.port) && p.putUChar("rotation",next.rotation) &&
         p.putUChar("panel",next.panel) && p.putBool("mq1",next.mq1) && p.putBool("mq2",next.mq2);
}
bool Configuration::openPortal() {
  if(portalActive) return true;
  char password[13], token[25];
  snprintf(password,sizeof(password),"%08lX%04lX",(unsigned long)esp_random(),(unsigned long)(esp_random()&0xffff));
  snprintf(token,sizeof(token),"%08lX%08lX%08lX",(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random());
  token_=token;apPassword=password;apName="ZLC-"+WiFi.macAddress().substring(12);apName.replace(":","");
  WiFi.mode(WIFI_AP_STA);
  if(!WiFi.softAP(apName.c_str(),apPassword.c_str(),1,false,2)) {message="配网热点启动失败";return false;}
  server_.on("/",HTTP_GET,[this]{page();});server_.on("/save",HTTP_POST,[this]{accept();});
  server_.onNotFound([this]{server_.send(404,"text/plain; charset=utf-8","Not found");});
  server_.begin();portalActive=true;
  Serial.printf("Setup AP: %s; temporary AP password: %s; URL: http://192.168.4.1\n",apName.c_str(),apPassword.c_str());
  return true;
}
void Configuration::page() {
  PortalFields f;f.mac=WiFi.macAddress().c_str();f.token=token_.c_str();
  f.ssid=value.ssid.c_str();f.group=value.group.c_str();f.name=value.name.c_str();f.host=value.host.c_str();
  f.port=value.port;f.rotation=value.rotation;f.panel=value.panel;
  f.mq1=value.mq1;f.mq2=value.mq2;
  f.cloudHost=value.cloud.host.c_str();f.hasCloudCredentials=value.cloud.configured();
  auto html=buildPortalHtml(f);
  server_.sendHeader("Cache-Control","no-store");server_.send(200,"text/html; charset=utf-8",html.c_str());
}
static bool safeIdentifier(const String& s,size_t max,bool host=false) {
  if(s.isEmpty()||s.length()>max) return false;
  for(unsigned i=0;i<s.length();i++) if(!isalnum((unsigned char)s[i])&&s[i]!='-'&&s[i]!='_'&&(!host||s[i]!='.')) return false;
  return true;
}
static bool validCredential(const String& value) {
  if (value.isEmpty() || value.length()>128) return false;
  for (unsigned i=0;i<value.length();i++) {
    if (value[i]<0x21 || value[i]>0x7e) return false;
  }
  return true;
}
void Configuration::accept() {
  if(!server_.hasArg("token")||server_.arg("token")!=token_) {server_.send(403,"text/plain","Invalid setup token");return;}
  if (server_.hasArg("demo_answers") || server_.hasArg("demo_clear")) {
    server_.send(400,"text/plain; charset=utf-8","此版本已停用演示答案设置。请刷新配网页。");
    return;
  }
  DeviceConfig next=value;
  next.ssid=server_.arg("ssid");next.group=server_.arg("group");next.group.trim();
  next.name=server_.arg("name");next.host=server_.arg("host");next.host.trim();
  if(server_.hasArg("openwifi")) next.wifiPassword="";
  else if(!server_.arg("wifi").isEmpty()) next.wifiPassword=server_.arg("wifi");
  if(!server_.arg("mqpass").isEmpty()) next.mqttPassword=server_.arg("mqpass");
  if (server_.hasArg("cloud_off")) next.cloud=CloudConfig{};
  else {
    next.cloud.host=server_.arg("tc_host");
    next.cloud.host.trim();
    if (next.cloud.host.startsWith("mqtt://")) next.cloud.host.remove(0,7);
    if (next.cloud.host.endsWith(":1883")) next.cloud.host.remove(next.cloud.host.length()-5);
    const auto token=server_.arg("tc_token"), key=server_.arg("tc_key");
    if (!token.isEmpty()) next.cloud.accessToken=token;
    if (!key.isEmpty()) next.cloud.projectKey=key;
    const bool edited=!next.cloud.host.isEmpty() || !token.isEmpty() || !key.isEmpty();
    if (edited && (!safeIdentifier(next.cloud.host,127,true) ||
        !validCredential(next.cloud.accessToken) || !validCredential(next.cloud.projectKey) ||
        (next.cloud.host!=value.cloud.host && (token.isEmpty() || key.isEmpty())))) {
      server_.send(400,"text/plain; charset=utf-8","ThingsCloud 设置无效。请填写 MQTT 接入点、AccessToken 和 ProjectKey。");
      return;
    }
    if (!edited) next.cloud=CloudConfig{};
  }
  long port=server_.arg("port").toInt();int r=server_.arg("rotation").toInt(),panel=server_.arg("panel").toInt();
  if(!next.usable()||next.ssid.length()>32||next.wifiPassword.length()>64||next.name.length()>64||
     (!next.group.isEmpty() && (!safeIdentifier(next.group,20)||!next.examConfigured()))||
     !safeIdentifier(next.host,63,true)||next.mqttPassword.length()>64||
     port<1||port>65535||(r!=1&&r!=3)||panel<0||panel>2) {
    server_.send(400,"text/plain; charset=utf-8","设置无效。检查Wi-Fi、组号、密码、服务器和屏幕参数；返回修改。");return;
  }
  next.port=port;next.rotation=r;next.panel=panel;next.mq1=server_.hasArg("mq1");next.mq2=server_.hasArg("mq2");
  if (!save(next)) {
    server_.send(500,"text/plain; charset=utf-8","设备存储写入失败，请重新保存。");
    return;
  }
  value=next;message="设置已保存，正在重启";rebootAt_=millis()+2000;
  server_.send(200,"text/html; charset=utf-8","<meta charset='utf-8'><h2>设置已保存，手柄即将重启</h2><p>手机可退出设备热点。设备会连接指定Wi-Fi。</p>");
}
void Configuration::tick() {
  if(portalActive) server_.handleClient();
  if(rebootAt_ && int32_t(millis()-rebootAt_)>=0) ESP.restart();
}
