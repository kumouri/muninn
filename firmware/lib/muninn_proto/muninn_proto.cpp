#include "muninn_proto.h"

namespace muninn::proto {

namespace {
inline void put_u16(uint8_t* p, uint16_t v) {
  p[0] = uint8_t(v & 0xFF);
  p[1] = uint8_t((v >> 8) & 0xFF);
}
inline void put_u32(uint8_t* p, uint32_t v) {
  p[0] = uint8_t(v & 0xFF);
  p[1] = uint8_t((v >> 8) & 0xFF);
  p[2] = uint8_t((v >> 16) & 0xFF);
  p[3] = uint8_t((v >> 24) & 0xFF);
}
inline uint16_t get_u16(const uint8_t* p) {
  return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}
inline uint32_t get_u32(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
         (uint32_t(p[3]) << 24);
}
}  // namespace

size_t encode(uint8_t* buf, size_t cap, uint8_t type, uint8_t flags, uint32_t seq,
              uint32_t timestamp_ms, const uint8_t* payload, uint16_t payload_len) {
  if (payload_len > MAX_PAYLOAD) return 0;
  const size_t total = HEADER_SIZE + payload_len;
  if (cap < total) return 0;

  buf[0] = MAGIC0;
  buf[1] = MAGIC1;
  buf[2] = VERSION;
  buf[3] = type;
  buf[4] = flags;
  buf[5] = 0;  // reserved
  put_u32(buf + 6, seq);
  put_u32(buf + 10, timestamp_ms);
  put_u16(buf + 14, payload_len);
  for (uint16_t i = 0; i < payload_len; ++i) buf[HEADER_SIZE + i] = payload[i];
  return total;
}

Decode decode(const uint8_t* buf, size_t len, Frame* out, size_t* consumed) {
  *consumed = 0;
  if (len < 1) return Decode::INCOMPLETE;

  // Resync: if the first byte can't start a valid magic, tell the caller how far to skip.
  if (buf[0] != MAGIC0) {
    size_t skip = 1;
    while (skip < len && buf[skip] != MAGIC0) ++skip;
    *consumed = skip;
    return Decode::BAD_MAGIC;
  }
  if (len < 2) return Decode::INCOMPLETE;  // have MAGIC0, need MAGIC1
  if (buf[1] != MAGIC1) {
    *consumed = 1;  // skip this stray MAGIC0 and keep scanning
    return Decode::BAD_MAGIC;
  }
  if (len < HEADER_SIZE) return Decode::INCOMPLETE;
  if (buf[2] != VERSION) {
    *consumed = 2;  // bad version behind a valid magic; skip past it
    return Decode::BAD_MAGIC;
  }

  const uint16_t payload_len = get_u16(buf + 14);
  if (payload_len > MAX_PAYLOAD) {
    *consumed = 2;  // implausible length — treat as noise and resync
    return Decode::BAD_MAGIC;
  }
  const size_t total = HEADER_SIZE + payload_len;
  if (len < total) return Decode::INCOMPLETE;

  out->hdr.version = buf[2];
  out->hdr.type = buf[3];
  out->hdr.flags = buf[4];
  out->hdr.seq = get_u32(buf + 6);
  out->hdr.timestamp_ms = get_u32(buf + 10);
  out->hdr.payload_len = payload_len;
  out->payload = buf + HEADER_SIZE;
  *consumed = total;
  return Decode::OK;
}

}  // namespace muninn::proto
