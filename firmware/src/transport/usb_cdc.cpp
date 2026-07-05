#include "transport/usb_cdc.h"

#include <Arduino.h>

namespace muninn {

bool UsbCdcTransport::begin() {
  Serial.begin(115200);  // USB CDC (ARDUINO_USB_CDC_ON_BOOT=1)
  return true;
}

size_t UsbCdcTransport::sendFrame(const uint8_t* frame, size_t len) {
  return Serial.write(frame, len);
}

size_t UsbCdcTransport::poll(uint8_t* buf, size_t cap) {
  size_t n = 0;
  while (n < cap && Serial.available() > 0) buf[n++] = static_cast<uint8_t>(Serial.read());
  return n;
}

}  // namespace muninn
