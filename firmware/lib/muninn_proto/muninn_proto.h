// Muninn wire-protocol codec — see docs/protocol.md (authoritative).
// Portable C++17, no hardware dependencies, so it compiles on-target and on the host (env:native).
#pragma once
#include <cstddef>
#include <cstdint>

namespace muninn::proto {

constexpr uint8_t MAGIC0 = 0x4D;  // 'M'
constexpr uint8_t MAGIC1 = 0x4E;  // 'N'
constexpr uint8_t VERSION = 1;
constexpr size_t  HEADER_SIZE = 16;
constexpr uint16_t MAX_PAYLOAD = 8192;

constexpr int SAMPLE_RATE_HZ = 16000;
constexpr int FRAME_MS = 20;                 // 320 samples / 640 bytes per AUDIO frame
constexpr int FRAME_SAMPLES = SAMPLE_RATE_HZ * FRAME_MS / 1000;  // 320

enum Type : uint8_t { AUDIO = 1, CONTROL = 2, TEXT_ACK = 3 };
enum Flag : uint8_t { CAPTURING = 0x01 };
enum Ctrl : uint8_t { CAPTURE_START = 1, CAPTURE_STOP = 2, PING = 3, PONG = 4 };

struct Header {
  uint8_t  version;
  uint8_t  type;
  uint8_t  flags;
  uint32_t seq;
  uint32_t timestamp_ms;
  uint16_t payload_len;
};

struct Frame {
  Header         hdr;
  const uint8_t* payload;  // points into the caller's buffer
};

// Encode one frame into `buf`. Returns total bytes written (HEADER_SIZE + payload_len),
// or 0 if `cap` is too small or payload_len exceeds MAX_PAYLOAD.
size_t encode(uint8_t* buf, size_t cap, uint8_t type, uint8_t flags, uint32_t seq,
              uint32_t timestamp_ms, const uint8_t* payload, uint16_t payload_len);

// Decode result codes for `decode()`.
enum class Decode { OK, INCOMPLETE, BAD_MAGIC };

// Try to parse one frame at the start of buf[0..len).
//  - OK:         *out filled, *consumed = HEADER_SIZE + payload_len.
//  - INCOMPLETE: not enough bytes yet; *consumed = 0.
//  - BAD_MAGIC:  first byte(s) are not a valid header; *consumed = number of bytes to skip to resync.
Decode decode(const uint8_t* buf, size_t len, Frame* out, size_t* consumed);

}  // namespace muninn::proto
