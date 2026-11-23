// Host adapter for the independent monitor regression. The runner inserts the
// production sampling, local-cycle, report and cloud callback functions unchanged.
#include <ArduinoJson.h>
#include <cassert>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include "BCfg.h"
#include "CloudSet.h"
using namespace bmon;
constexpr int HIGH=1, LOW=0, OUTPUT=3;
static int raw=1200, mv=990, reads=0, badAt=-1;
int analogRead(int) { return reads++ == badAt ? 4095 : raw; }
uint32_t analogReadMilliVolts(int) { return mv; }
static bool attachOK[64], writeOK[64];
static int pwm[64], gpio[64];
void pinMode(int,int) {}
void digitalWrite(int p,int v) { gpio[p]=v; }
bool ledcAttach(int p,int,int) { return attachOK[p]; }
bool ledcWrite(int p,uint32_t val) { if (!writeOK[p]) return false; pwm[p]=val; return true; }
#include "MonitorIO.h"
static uint32_t now=4000;
uint32_t millis() { return now; }
struct {float t=20,h=50; float readTemperature(){return t;} float readHumidity(){return h;}} dht;
using StaticSemaphore_t=int; using SemaphoreHandle_t=int; constexpr int portMAX_DELAY=0;
int locked=0;
void xSemaphoreTake(int,int){assert(locked++==0);} void xSemaphoreGive(int){assert(locked--==1);}
struct {template<class... Args> void printf(const char*,Args...){} void println(const char*){}} Serial;
using String=std::string;
struct {std::string last;bool publish(const char*,const char* text,bool){assert(!locked);last=text;return true;}} mqtt;
std::string topicUp="up",topicSet="set",topicReply="reply";
uint32_t msgSeq=0;
// A bounded RTOS delay drives the real worker entry with the network loop stalled.
struct EndSlice {};
int slices = 0;
uint32_t pdMS_TO_TICKS(uint32_t ms){return ms;}
void vTaskDelay(uint32_t ms){assert(ms==2000);if(!slices)throw EndSlice{};--slices;now+=ms;}
void runSamples(int count);
#include "monitor_functions.inc"
void runSamples(int count){slices=count;try{sampleTask(nullptr);}catch(const EndSlice&){}assert(!slices);}

JsonDocument decoded(){JsonDocument j;assert(!deserializeJson(j,mqtt.last));return j;}
int down(std::string text){onMessage(topicSet.data(),reinterpret_cast<uint8_t*>(text.data()),text.size());return decoded()["code"].as<int>();}
int main(){
  std::fill_n(attachOK,64,true); std::fill_n(writeOK,64,true);
  assert(readMq(-1)==-1 && reads==0);
  assert(readMq(1)==300); mv=989; assert(readMq(1)==300); raw=4090;assert(readMq(1)==-1);raw=1000;
  mv=3050;assert(readMq(1)==-1);mv=3049;assert(readMq(1)>=0);
  badAt=reads+15;assert(readMq(1)==-1);badAt=-1;mv=0;assert(readMq(1)==0);
  assert(validDht(0,0)&&validDht(50,100));assert(!validDht(51,50)&&!validDht(25,101)&&!validDht(NAN,45)&&!validDht(21,INFINITY));
  outputs.begin();assert(outputs.fan.confirmed==0&&outputs.servo.confirmed==0);
  // These local cycles exercise the actual firmware functions with no network calls.
  dht.t=35;mv=1800;const auto start=now;runSamples(5);assert(now-start==10000 && sampleAt==now);assert(outputs.fan.confirmed==(kVerify?70:80));assert(outputs.servo.confirmed==(kVerify?0:90));
  dht.t=NAN;mv=3050;localCycle();report();auto j=decoded();auto p=j["params"].as<JsonObject>();
  for(auto key:{"temperature","humidity","air","smoke"}) assert(p[key].isNull());
  assert(p["fan_level"]["value"].as<int>()==(kVerify?70:80));
  dht.t=20;mv=0;localCycle();assert(outputs.fan.confirmed==0&&outputs.servo.confirmed==0);
  now=0xfffffff0u;assert(down(R"({"id":"1","params":{"servo":45}})")==200);
  dht.t=40;now+=2000;localCycle();assert(outputs.fan.confirmed==100&&outputs.servo.confirmed==45);
  now+=28000;localCycle();assert(outputs.servo.confirmed==(kVerify?45:0));
  writeOK[FanPwm]=false;assert(down(R"({"id":"2","params":{"fan_level":55}})")==503);assert(outputs.fan.confirmed==-1);
  if(FanIn1>=0)assert(gpio[FanIn1]==LOW);report();assert(decoded()["params"]["fan_level"].isNull());
  writeOK[FanPwm]=true;localCycle();assert(outputs.fan.confirmed==100);
  writeOK[ServoPwm]=false;assert(down(R"({"id":"3","params":{"fan_level":65,"servo":120}})")==503);
  assert(outputs.fan.confirmed==65&&outputs.servo.confirmed==-1);report();assert(decoded()["params"]["servo"].isNull());
  writeOK[ServoPwm]=true;localCycle();assert(outputs.fan.confirmed==65&&outputs.servo.confirmed==0);
  attachOK[FanPwm]=false; MonitorOutputs other;other.begin();assert(other.fan.confirmed==-1&&other.servo.confirmed==0);
  assert(!other.command(90,120,now)&&other.servo.confirmed==0);assert(other.command(-1,120,now));
  attachOK[ServoPwm]=false;attachOK[FanPwm]=true;MonitorOutputs third;third.begin();assert(third.servo.confirmed==-1&&third.command(40,-1,now));
  const auto last=mqtt.last;now+=5001;report();assert(last==mqtt.last);
  std::printf("PASS %s: ADC, DHT, offline linkage, PWM failures, MQTT reply/report, manual expiry and wraparound\n",BoardName);
}
