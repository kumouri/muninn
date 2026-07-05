// usb_cdc — v1 default transport. Rides the same USB cable as the UAC audio (composite device):
// frames go to the PC over the CDC-ACM serial endpoint. No radio, no coexistence concerns.
#pragma once
#include "transport/transport.h"

namespace muninn {

class UsbCdcTransport : public Transport {
 public:
  bool begin() override;
  size_t sendFrame(const uint8_t* frame, size_t len) override;
  size_t poll(uint8_t* buf, size_t cap) override;
  const char* name() const override { return "usb-cdc"; }
};

}  // namespace muninn
