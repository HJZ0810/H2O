#include "Config.h"
#include "PortalHtml.h"
#include <Preferences.h>
#include <WiFi.h>
#include <esp_system.h>
void Configuration::load() {
  Preferences p;p.begin("zlc-config",true);
  value.ssid=p.getString("ssid","");value.wifiPassword=p.getString("wifi","");
  value.group=p.getString("group","");value.mqttPassword=p.getString("mqpass","");
  value.name=p.getString("name","手柄");value.host=p.getString("host","124.70.71.221");
  value.port=p.getUShort("port",1883);value.rotation=p.getUChar("rotation",1);value.panel=p.getUChar("panel",0);
  value.mq1=p.getBool("mq1",false);value.mq2=p.getBool("mq2",false);p.end();
}
void Configuration::save() {
  Preferences p;p.begin("zlc-config",false);
  p.putString("ssid",value.ssid);p.putString("wifi",value.wifiPassword);p.putString("group",value.group);
  p.putString("mqpass",value.mqttPassword);p.putString("name",value.name);p.putString("host",value.host);
  p.putUShort("port",value.port);p.putUChar("rotation",value.rotation);p.putUChar("panel",value.panel);
  p.putBool("mq1",value.mq1);p.putBool("mq2",value.mq2);p.end();
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
  f.mq1=value.mq1;f.mq2=value.mq2;f.hasMqttPassword=!value.mqttPassword.isEmpty();
  auto html=buildPortalHtml(f);
  server_.sendHeader("Cache-Control","no-store");server_.send(200,"text/html; charset=utf-8",html.c_str());
}
static bool safeIdentifier(const String& s,size_t max,bool host=false) {
  if(s.isEmpty()||s.length()>max) return false;
  for(unsigned i=0;i<s.length();i++) if(!isalnum((unsigned char)s[i])&&s[i]!='-'&&s[i]!='_'&&(!host||s[i]!='.')) return false;
  return true;
}
void Configuration::accept() {
  if(!server_.hasArg("token")||server_.arg("token")!=token_) {server_.send(403,"text/plain","Invalid setup token");return;}
  DeviceConfig next=value;
  next.ssid=server_.arg("ssid");next.group=server_.arg("group");next.group.trim();
  next.name=server_.arg("name");next.host=server_.arg("host");next.host.trim();
  if(server_.hasArg("openwifi")) next.wifiPassword="";
  else if(!server_.arg("wifi").isEmpty()) next.wifiPassword=server_.arg("wifi");
  if(!server_.arg("mqpass").isEmpty()) next.mqttPassword=server_.arg("mqpass");
  long port=server_.arg("port").toInt();int r=server_.arg("rotation").toInt(),panel=server_.arg("panel").toInt();
  if(next.ssid.isEmpty()||next.ssid.length()>32||next.wifiPassword.length()>64||next.name.length()>64||
     !safeIdentifier(next.group,20)||!safeIdentifier(next.host,63,true)||next.mqttPassword.isEmpty()||next.mqttPassword.length()>64||
     port<1||port>65535||(r!=1&&r!=3)||panel<0||panel>2) {
    server_.send(400,"text/plain; charset=utf-8","设置无效。检查Wi-Fi、组号、密码、服务器和屏幕参数；返回修改。");return;
  }
  next.port=port;next.rotation=r;next.panel=panel;next.mq1=server_.hasArg("mq1");next.mq2=server_.hasArg("mq2");
  value=next;save();message="设置已保存，正在重启";rebootAt_=millis()+2000;
  server_.send(200,"text/html; charset=utf-8","<meta charset='utf-8'><h2>设置已保存，手柄即将重启</h2><p>手机可退出设备热点。设备会连接指定Wi-Fi。</p>");
}
void Configuration::tick() {
  if(portalActive) server_.handleClient();
  if(rebootAt_ && int32_t(millis()-rebootAt_)>=0) ESP.restart();
}
