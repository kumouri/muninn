#include <unity.h>

#include "muninn_dsp.h"

using namespace muninn::dsp;

void test_downsample_mono_factor_3() {
  // 6 mono frames -> 2 output samples, each the average of 3 inputs.
  const int16_t in[] = {300, 300, 300, 900, 900, 900};
  int16_t out[8] = {0};
  size_t n = downsample_48k_to_16k(in, 6, 1, out, 8);
  TEST_ASSERT_EQUAL_UINT(2, n);
  TEST_ASSERT_EQUAL_INT16(300, out[0]);
  TEST_ASSERT_EQUAL_INT16(900, out[1]);
}

void test_downsample_drops_partial_group() {
  // 7 frames -> 2 complete groups; the 7th is dropped.
  const int16_t in[] = {0, 0, 0, 30, 60, 90, 12345};
  int16_t out[8] = {0};
  size_t n = downsample_48k_to_16k(in, 7, 1, out, 8);
  TEST_ASSERT_EQUAL_UINT(2, n);
  TEST_ASSERT_EQUAL_INT16(0, out[0]);
  TEST_ASSERT_EQUAL_INT16(60, out[1]);  // (30+60+90)/3
}

void test_downsample_stereo_downmix() {
  // Stereo: each frame is L,R -> mono average, then 3-tap average.
  // frames: (100,300)->200, (200,400)->300, (300,500)->400 ; avg = 300
  const int16_t in[] = {100, 300, 200, 400, 300, 500};
  int16_t out[4] = {0};
  size_t n = downsample_48k_to_16k(in, 3, 2, out, 4);
  TEST_ASSERT_EQUAL_UINT(1, n);
  TEST_ASSERT_EQUAL_INT16(300, out[0]);
}

void test_downsample_respects_out_cap() {
  const int16_t in[] = {1, 1, 1, 2, 2, 2, 3, 3, 3};
  int16_t out[1] = {0};
  size_t n = downsample_48k_to_16k(in, 9, 1, out, 1);
  TEST_ASSERT_EQUAL_UINT(1, n);  // capped
}

void test_downmix_to_mono() {
  const int16_t in[] = {10, 20, 30, 40};  // two stereo frames
  int16_t out[2] = {0};
  size_t n = downmix_to_mono(in, 2, 2, out, 2);
  TEST_ASSERT_EQUAL_UINT(2, n);
  TEST_ASSERT_EQUAL_INT16(15, out[0]);
  TEST_ASSERT_EQUAL_INT16(35, out[1]);
}

void test_mix_to_mono_unity_sums_channels() {
  // program (ch0) + voice (ch1) at unity gain -> sum.
  const int16_t in[] = {100, 200, -50, 50};  // two stereo frames
  int16_t out[2] = {0};
  size_t n = mix_to_mono_q8(in, 2, 2, /*g0=*/256, /*g1=*/256, out, 2);
  TEST_ASSERT_EQUAL_UINT(2, n);
  TEST_ASSERT_EQUAL_INT16(300, out[0]);  // 100 + 200
  TEST_ASSERT_EQUAL_INT16(0, out[1]);    // -50 + 50
}

void test_mix_to_mono_voice_emphasis() {
  // Half the program, full voice -> emphasize your own voice.
  const int16_t in[] = {200, 100};  // one stereo frame: program=200, voice=100
  int16_t out[1] = {0};
  size_t n = mix_to_mono_q8(in, 1, 2, /*g0=*/128, /*g1=*/256, out, 1);
  TEST_ASSERT_EQUAL_UINT(1, n);
  TEST_ASSERT_EQUAL_INT16(200, out[0]);  // 200*0.5 + 100*1.0 = 100 + 100
}

void test_mix_to_mono_clamps() {
  const int16_t in[] = {30000, 30000};  // sum would overflow int16
  int16_t out[1] = {0};
  mix_to_mono_q8(in, 1, 2, 256, 256, out, 1);
  TEST_ASSERT_EQUAL_INT16(32767, out[0]);
}

void test_downsample_stereo_keeps_channels_separate() {
  // 3 stereo frames -> 1 stereo output frame (2 samples). L=100 const, R=900 const.
  const int16_t in[] = {100, 900, 100, 900, 100, 900};
  int16_t out[4] = {0};
  size_t n = downsample_48k_to_16k_stereo(in, 3, out, 4);
  TEST_ASSERT_EQUAL_UINT(2, n);
  TEST_ASSERT_EQUAL_INT16(100, out[0]);  // L
  TEST_ASSERT_EQUAL_INT16(900, out[1]);  // R
}

void test_measure_levels_stereo_peaks() {
  // two stereo frames: ch0 peaks at 500, ch1 peaks at -900 -> |900|
  const int16_t in[] = {100, -200, 500, -900};
  Levels lv = measure_levels(in, 2, 2, /*clip=*/32000);
  TEST_ASSERT_EQUAL_INT16(500, lv.peak[0]);
  TEST_ASSERT_EQUAL_INT16(900, lv.peak[1]);
  TEST_ASSERT_FALSE(lv.clip[0]);
  TEST_ASSERT_FALSE(lv.clip[1]);
}

void test_measure_levels_clip_flag() {
  const int16_t in[] = {32000, 10};  // one stereo frame; ch0 at clip threshold
  Levels lv = measure_levels(in, 1, 2, /*clip=*/32000);
  TEST_ASSERT_TRUE(lv.clip[0]);
  TEST_ASSERT_FALSE(lv.clip[1]);
}

void test_measure_levels_handles_int16_min() {
  const int16_t in[] = {-32768, 0};  // must not overflow when abs'd
  Levels lv = measure_levels(in, 1, 1, 32000);
  TEST_ASSERT_EQUAL_INT16(32767, lv.peak[0]);
  TEST_ASSERT_TRUE(lv.clip[0]);
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_downsample_mono_factor_3);
  RUN_TEST(test_downsample_drops_partial_group);
  RUN_TEST(test_downsample_stereo_downmix);
  RUN_TEST(test_downsample_respects_out_cap);
  RUN_TEST(test_downmix_to_mono);
  RUN_TEST(test_mix_to_mono_unity_sums_channels);
  RUN_TEST(test_mix_to_mono_voice_emphasis);
  RUN_TEST(test_mix_to_mono_clamps);
  RUN_TEST(test_measure_levels_stereo_peaks);
  RUN_TEST(test_measure_levels_clip_flag);
  RUN_TEST(test_measure_levels_handles_int16_min);
  RUN_TEST(test_downsample_stereo_keeps_channels_separate);
  return UNITY_END();
}
