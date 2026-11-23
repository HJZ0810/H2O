#pragma once
#include <WiFi.h>
#if __has_include("OneNetTrust.h")
#include "OneNetTrust.h"
#include <WiFiClientSecure.h>
namespace onetransport {
using Client = WiFiClientSecure;
constexpr uint16_t Port = 8883;
inline void prepare(Client& client) { client.setCACert(H2O_ONENET_CA); }
}
#else
namespace onetransport {
using Client = WiFiClient;
constexpr uint16_t Port = 1883;
inline void prepare(Client&) {}
}
#endif

namespace onetransport {

// PubSubClient reports SUBSCRIBE writes but silently consumes SUBACK packets.
// Observe just this session's single subscription on the existing transport.
class SubscriptionClient : public Client {
 public:
  enum class Ack { Idle, Pending, Accepted, Rejected };
  using Transport = onetransport::Client;
  using Transport::read;
  using Transport::write;

  void resetSubscription() {
    ack_ = Ack::Idle;
    captureId_ = false;
    packetId_ = 0;
    phase_ = Phase::Header;
  }

  void expectSubscription() {
    resetSubscription();
    captureId_ = true;
  }

  Ack subscriptionAck() const { return ack_; }

  size_t write(const uint8_t* data, size_t size) override {
    if (captureId_ && size && data[0] == 0x82) {
      captureId_ = false;
      size_t i = 1;
      // Skip the MQTT Remaining Length field to get the packet identifier.
      while (i < size && i <= 4 && (data[i++] & 0x80)) {}
      if (i <= 5 && i + 1 < size && !(data[i - 1] & 0x80)) {
        packetId_ = uint16_t(data[i]) << 8 | data[i + 1];
        if (packetId_) ack_ = Ack::Pending;
      }
    }
    return Transport::write(data, size);
  }

  int read() override {
    const int value = Transport::read();
    if (value >= 0 && ack_ == Ack::Pending) observe(uint8_t(value));
    return value;
  }

 private:
  enum class Phase { Header, Length, Body };
  Ack ack_ = Ack::Idle;
  Phase phase_ = Phase::Header;
  bool captureId_ = false;
  uint16_t packetId_ = 0, incomingId_ = 0;
  uint8_t header_ = 0, lengthBytes_ = 0;
  uint32_t remaining_ = 0;

  void observe(uint8_t byte) {
    switch (phase_) {
      case Phase::Header:
        header_ = byte;
        remaining_ = lengthBytes_ = 0;
        phase_ = Phase::Length;
        break;
      case Phase::Length:
        remaining_ |= uint32_t(byte & 0x7f) << (7 * lengthBytes_++);
        if (!(byte & 0x80)) {
          phase_ = remaining_ ? Phase::Body : Phase::Header;
          // Exactly one return code is expected for a single subscribed topic.
          if (header_ == 0x90 && remaining_ != 3) ack_ = Ack::Rejected;
        } else if (lengthBytes_ == 4) {
          ack_ = Ack::Rejected;
        }
        break;
      case Phase::Body:
        if (header_ == 0x90) {
          if (remaining_ == 3) incomingId_ = uint16_t(byte) << 8;
          else if (remaining_ == 2) incomingId_ |= byte;
          else if (incomingId_ == packetId_) ack_ = byte <= 1 ? Ack::Accepted : Ack::Rejected;
        }
        if (--remaining_ == 0) phase_ = Phase::Header;
        break;
    }
  }
};

}  // namespace onetransport
