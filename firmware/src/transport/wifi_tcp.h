// wifi_tcp — M2 transport. Streams frames to the listener PC over TCP (the "wireless PC" case).
#pragma once
#include "transport/transport.h"

namespace muninn {

class WifiTcpTransport : public Transport {
 public:
  WifiTcpTransport(const char* host, uint16_t port) : host_(host), port_(port) {}
  bool begin() override;
  size_t sendFrame(const uint8_t* frame, size_t len) override;
  size_t poll(uint8_t* buf, size_t cap) override;
  const char* name() const override { return "wifi-tcp"; }

 private:
  const char* host_;
  uint16_t port_;
};

}  // namespace muninn
