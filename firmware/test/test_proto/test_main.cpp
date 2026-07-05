#include <unity.h>

#include <cstring>

#include "muninn_proto.h"

using namespace muninn::proto;

static uint8_t buf[256];

void test_encode_decode_roundtrip() {
  const uint8_t pcm[] = {0x11, 0x22, 0x33, 0x44};
  size_t n = encode(buf, sizeof(buf), AUDIO, CAPTURING, 0xDEADBEEF, 12345, pcm, sizeof(pcm));
  TEST_ASSERT_EQUAL_UINT(HEADER_SIZE + sizeof(pcm), n);

  Frame f;
  size_t consumed = 0;
  Decode d = decode(buf, n, &f, &consumed);
  TEST_ASSERT_EQUAL_INT((int)Decode::OK, (int)d);
  TEST_ASSERT_EQUAL_UINT(n, consumed);
  TEST_ASSERT_EQUAL_UINT8(VERSION, f.hdr.version);
  TEST_ASSERT_EQUAL_UINT8(AUDIO, f.hdr.type);
  TEST_ASSERT_EQUAL_UINT8(CAPTURING, f.hdr.flags);
  TEST_ASSERT_EQUAL_UINT32(0xDEADBEEF, f.hdr.seq);
  TEST_ASSERT_EQUAL_UINT32(12345, f.hdr.timestamp_ms);
  TEST_ASSERT_EQUAL_UINT16(sizeof(pcm), f.hdr.payload_len);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(pcm, f.payload, sizeof(pcm));
}

void test_decode_incomplete() {
  const uint8_t pcm[] = {1, 2, 3, 4, 5, 6};
  size_t n = encode(buf, sizeof(buf), AUDIO, 0, 1, 0, pcm, sizeof(pcm));
  Frame f;
  size_t consumed = 0;
  // Truncate mid-payload.
  Decode d = decode(buf, n - 2, &f, &consumed);
  TEST_ASSERT_EQUAL_INT((int)Decode::INCOMPLETE, (int)d);
  TEST_ASSERT_EQUAL_UINT(0, consumed);
}

void test_decode_resync_after_garbage() {
  uint8_t stream[300];
  // 3 bytes of noise, then a real frame.
  stream[0] = 0x00;
  stream[1] = 0x4D;  // stray MAGIC0 not followed by MAGIC1
  stream[2] = 0x99;
  const uint8_t pcm[] = {0x55, 0x66};
  size_t framelen = encode(stream + 3, sizeof(stream) - 3, CONTROL, 0, 7, 100, pcm, sizeof(pcm));
  size_t total = 3 + framelen;

  // First decode: should report BAD_MAGIC and skip forward (not zero, not the whole frame).
  Frame f;
  size_t consumed = 0;
  Decode d = decode(stream, total, &f, &consumed);
  TEST_ASSERT_EQUAL_INT((int)Decode::BAD_MAGIC, (int)d);
  TEST_ASSERT_TRUE(consumed >= 1);

  // Walk the resync until we land on the good frame.
  size_t off = 0;
  int guard = 0;
  while (off < total && guard++ < 50) {
    d = decode(stream + off, total - off, &f, &consumed);
    if (d == Decode::OK) break;
    TEST_ASSERT_TRUE(consumed >= 1);  // must always make progress
    off += consumed;
  }
  TEST_ASSERT_EQUAL_INT((int)Decode::OK, (int)d);
  TEST_ASSERT_EQUAL_UINT8(CONTROL, f.hdr.type);
  TEST_ASSERT_EQUAL_UINT32(7, f.hdr.seq);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(pcm, f.payload, sizeof(pcm));
}

void test_encode_rejects_small_buffer() {
  const uint8_t pcm[] = {1, 2, 3, 4};
  TEST_ASSERT_EQUAL_UINT(0, encode(buf, 5, AUDIO, 0, 0, 0, pcm, sizeof(pcm)));
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_encode_decode_roundtrip);
  RUN_TEST(test_decode_incomplete);
  RUN_TEST(test_decode_resync_after_garbage);
  RUN_TEST(test_encode_rejects_small_buffer);
  return UNITY_END();
}
