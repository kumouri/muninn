// wifi_tcp — M2 transport. Streams frames to the listener PC over TCP (the "wireless PC" case).
// Reconnects (throttled) if the link drops. Credentials come from src/secrets.h (see .example).
#pragma once
#include "transport/transport.h"

namespace muninn {

class WifiTcpTransport : public Transport {
 public:
  WifiTcpTransport() = default;  // host/port come from config.h + secrets.h (MUNINN_LISTENER_*)
  bool begin() override;
  size_t sendFrame(const uint8_t* frame, size_t len) override;
  size_t poll(uint8_t* buf, size_t cap) override;
  const char* name() const override { return "wifi-tcp"; }

  bool connected();

 private:
  bool connect();
  uint32_t last_retry_ms_ = 0;
};

}  // namespace muninn
