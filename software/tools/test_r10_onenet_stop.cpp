#include <cassert>
#include <cstdio>
#include "ExamCore.h"
#include "OneNetCfg.h"

// Compile the firmware's actual control handlers and the OneNet portion of loop().
enum class View { Menu, Question, Story, Judge, Finished, Recovery, Sensor, Info, Setup, Fan, Monitor };
enum class Press { None, Short, Long };
View view = View::Menu;
int qScroll = 0, menuIndex = 5;
unsigned paintedRevision = 0, optionCursor = 0;
String lastKey;
exam::Core session;
struct { int lineStep = 1; } theme;
struct { int width() { return 0; } int height() { return 0; } } panel;
uint16_t framebuffer[1], *framebuf = framebuffer;
OneNet onenet;
Actuators fan;
bool onenetActive = false;
struct { void tick() {} } configuration;
Sensors sensors;
void Sensors::tick() {}
void tell(const char*) {}
void sendCommand(const std::string&) { assert(false && "exam path must not run"); }
#include "entry.inc"

static void serial(const char* cmd) {
  while (*cmd) Serial.input.push_back(*cmd++);
  Serial.input.push_back('\n');
  loop();
}

static void menuToggle() {
  view = View::Menu;
  menuIndex = 5;
  handleKey(board::Confirm, Press::Short);
  assert(view == View::Monitor);
  loop();
}

static void setOutput() {
  const std::string topic = "$sys/p/d/thing/property/set";
  const std::string json = R"({"id":"1","params":{"fan_level":80,"servo":90}})";
  std::vector<uint8_t> packet{0x30, uint8_t(2 + topic.size() + json.size()), 0, uint8_t(topic.size())};
  packet.insert(packet.end(), topic.begin(), topic.end());
  packet.insert(packet.end(), json.begin(), json.end());
  broker::push(packet);
}

static void assertStopped() {
  assert(!onenetActive && !onenet.connected() && !broker::up);
  assert(fan.speedPercent(false) == 0 && fan.speedPercent(true) == 0 && fan.pulseWidth() == 0);
  assert(level[board::MotorAin1] == LOW && level[board::MotorBin1] == LOW && pwm[board::ServoPwm] == 0);
  assert(!onenet.manualHold());
  assert(onenet.fanLevel() == 0 && onenet.servoAngle() == 0);
}

static void assertOpen() {
  assert(fan.speedPercent(false) == 80 && fan.servoPosition() == 0 && fan.pulseWidth() > 0);
}

int main(int argc, char** argv) {
  assert(argc == 2);
  const std::string test = argv[1];
  fan.begin();
  onenet.configure("p", "d", "eA==", "fake-broker");
  onenet.attach(&fan);
  sensors.mq[0] = {0, 1100, true, true, false};
  sensors.mq[1] = {0, 2200, true, true, false};
  WiFi.online = true;
  if (test == "short-write") broker::mode = broker::ShortWrite;
  else if (test == "reject") broker::mode = broker::Reject;
  else if (test == "wrong-id") broker::mode = broker::WrongId;
  else if (test == "timeout" || test == "early") broker::mode = broker::Silence;
  serial("onon");
  assert(onenetActive && broker::subscriptions == 1);
  assertOpen();

  if (test == "short-write" || test == "reject" || test == "timeout" || test == "wrong-id" || test == "early") {
    assert(!onenet.connected() && broker::reports == 0);
    if (test == "early") {
      setOutput();
      loop();
      assert(!onenet.manualHold() && broker::replies == 0);
      const auto id = broker::subscriptionId;
      broker::push({0x90, 3, uint8_t(id >> 8), uint8_t(id), 0});
      loop();
      assert(onenet.connected() && broker::reports == 1);
    } else {
      loop();
      if (test == "timeout" || test == "wrong-id") {
        assert(broker::up);
        clockMs += 5000;
        loop();
      }
      assert(!broker::up && !onenet.connected() && broker::reports == 0);
      loop();
      assert(broker::connects == 1);
      broker::mode = broker::Accept;
      clockMs += 5000;
      loop(); loop();
      assert(onenet.connected() && broker::subscriptions == 2 && broker::reports == 1);
    }
    setOutput(); loop();
    assert(onenet.manualHold() && broker::replies == 1);
  } else {
    loop();
    assert(onenet.connected());
    setOutput(); loop();
    assert(onenet.manualHold() && onenet.downlinkCount() == 1);
    if (test == "missing-sensor") {
      sensors.mq[0].clipped = sensors.mq[1].clipped = true;
      clockMs += 30000;
      loop();
      assertOpen();
      sensors.mq[0].clipped = sensors.mq[1].clipped = false;
      sensors.mq[0].mv = sensors.mq[1].mv = 0;
      loop();
      assert(fan.speedPercent(false) == 0 && fan.servoPosition() == -100);
    } else if (test == "offline") {
      WiFi.online = false;
      sensors.mq[0].mv = sensors.mq[1].mv = 0;
      loop();
      assert(!onenet.connected() && onenet.manualHold());
      assertOpen();
      clockMs += 30000;
      loop();
      assert(fan.speedPercent(false) == 0 && fan.servoPosition() == -100 && fan.pulseWidth() > 0);
      assert(broker::connects == 1);
      WiFi.online = true;
      loop(); loop();
      assert(onenet.connected() && broker::connects == 2);
    } else {
      if (test == "menu") menuToggle();
      else serial("onoff");
      assertStopped();
      WiFi.online = false;
      loop();
      assertStopped();
      menuToggle();
      assertOpen();
      assert(!onenet.manualHold() && onenet.downlinkCount() == 1);
      switchView(View::Fan);
      assertStopped();
      handleKey(board::Up, Press::Short);
      assert(fan.speedPercent(false) == 40);
      handleKey(board::Confirm, Press::Short);
      assertStopped();
      handleKey(board::Up, Press::Short);
      handleKey(board::Back, Press::Short);
      assertStopped();
      // A valid closed-window decision is also reapplied from PWM-off.
      sensors.mq[1].mv = 0;
      menuToggle();
      assert(fan.servoPosition() == -100 && fan.pulseWidth() > 0);
    }
  }
  std::printf("PASS %s\n", argv[1]);
}
