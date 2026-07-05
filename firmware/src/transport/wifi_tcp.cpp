#include "transport/wifi_tcp.h"

#include <Arduino.h>
#include <WiFi.h>

// secrets.h first so its MUNINN_LISTENER_HOST wins over the config.h #ifndef fallback.
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#include "config.h"

// M2 transport. Credentials live in src/secrets.h (git-ignored). Without them, begin() returns false
// and the caller can fall back to SD store-and-forward.

namespace muninn {

static WiFiClient s_client;

#ifndef MUNINN_WIFI_RECONNECT_MS
#define MUNINN_WIFI_RECONNECT_MS 2000
#endif

bool WifiTcpTransport::begin() {
#if defined(MUNINN_WIFI_SSID)
  WiFi.mode(WIFI_STA);
  WiFi.begin(MUNINN_WIFI_SSID, MUNINN_WIFI_PASS);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) delay(200);
  if (WiFi.status() != WL_CONNECTED) return false;
  return connect();
#else
  return false;  // no credentials configured — see secrets.h.example
#endif
}

bool WifiTcpTransport::connect() {
  if (s_client.connected()) return true;
  return s_client.connect(MUNINN_LISTENER_HOST, MUNINN_LISTENER_TCP_PORT);
}

bool WifiTcpTransport::connected() { return s_client.connected(); }

size_t WifiTcpTransport::sendFrame(const uint8_t* frame, size_t len) {
  if (!s_client.connected()) {
    // Throttle reconnect attempts so a down listener doesn't stall the audio loop.
    uint32_t now = millis();
    if (now - last_retry_ms_ < MUNINN_WIFI_RECONNECT_MS) return 0;
    last_retry_ms_ = now;
    if (!connect()) return 0;
  }
  return s_client.write(frame, len);
}

size_t WifiTcpTransport::poll(uint8_t* buf, size_t cap) {
  size_t n = 0;
  while (n < cap && s_client.available() > 0) buf[n++] = static_cast<uint8_t>(s_client.read());
  return n;
}

}  // namespace muninn
