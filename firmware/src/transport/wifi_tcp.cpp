#include "transport/wifi_tcp.h"

#include <Arduino.h>
#include <WiFi.h>

// M2 transport. NOTE: on the original-ESP32 BT relay (M3), Wi-Fi shares the 2.4 GHz radio with
// Classic BT — prefer sd_store during live A2DP, or accept reduced audio quality.

namespace muninn {

static WiFiClient s_client;

bool WifiTcpTransport::begin() {
  // Credentials come from src/secrets.h (git-ignored). See secrets.h.example.
#if __has_include("secrets.h")
#include "secrets.h"
  WiFi.mode(WIFI_STA);
  WiFi.begin(MUNINN_WIFI_SSID, MUNINN_WIFI_PASS);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) delay(200);
  if (WiFi.status() != WL_CONNECTED) return false;
  return s_client.connect(host_, port_);
#else
  return false;  // no credentials configured
#endif
}

size_t WifiTcpTransport::sendFrame(const uint8_t* frame, size_t len) {
  if (!s_client.connected()) return 0;
  return s_client.write(frame, len);
}

size_t WifiTcpTransport::poll(uint8_t* buf, size_t cap) {
  size_t n = 0;
  while (n < cap && s_client.available() > 0) buf[n++] = static_cast<uint8_t>(s_client.read());
  return n;
}

}  // namespace muninn
