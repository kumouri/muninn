// Transport — a sink that carries encoded protocol frames to the PC listener.
// Implementations: usb_cdc (v1 default), wifi_tcp (M2), sd_store (M2 store-and-forward).
#pragma once
#include <cstddef>
#include <cstdint>

namespace muninn {

class Transport {
 public:
  virtual ~Transport() = default;

  // Open the link. Returns false on failure.
  virtual bool begin() = 0;

  // Send one fully-encoded frame (header + payload). Returns bytes sent, or 0 on failure.
  virtual size_t sendFrame(const uint8_t* frame, size_t len) = 0;

  // Non-blocking receive of one inbound byte stream chunk (e.g. TEXT_ACK from the listener).
  // Returns bytes read into `buf` (0 if none available).
  virtual size_t poll(uint8_t* buf, size_t cap) = 0;

  virtual const char* name() const = 0;
};

}  // namespace muninn
