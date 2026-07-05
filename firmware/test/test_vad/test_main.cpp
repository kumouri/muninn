#include <unity.h>

#include "muninn_vad.h"

using namespace muninn;

void test_vad_starts_on_loud() {
  Vad vad(/*threshold=*/1000, /*hang_ms=*/500);
  TEST_ASSERT_EQUAL_INT((int)VadEvent::None, (int)vad.update(100, 0));      // quiet
  TEST_ASSERT_EQUAL_INT((int)VadEvent::Started, (int)vad.update(2000, 10));  // crosses
  TEST_ASSERT_TRUE(vad.active());
  TEST_ASSERT_EQUAL_INT((int)VadEvent::None, (int)vad.update(2000, 20));  // still loud, no re-trigger
}

void test_vad_stops_after_hangover() {
  Vad vad(1000, 500);
  vad.update(2000, 0);  // Started
  // Quiet but within hangover -> stay active.
  TEST_ASSERT_EQUAL_INT((int)VadEvent::None, (int)vad.update(100, 200));
  TEST_ASSERT_TRUE(vad.active());
  // Quiet past hangover -> Stopped.
  TEST_ASSERT_EQUAL_INT((int)VadEvent::Stopped, (int)vad.update(100, 500));
  TEST_ASSERT_FALSE(vad.active());
}

void test_vad_hangover_resets_on_new_loud() {
  Vad vad(1000, 500);
  vad.update(2000, 0);                 // Started
  vad.update(100, 300);                // quiet, within hangover
  vad.update(2000, 400);               // loud again -> resets last_loud
  TEST_ASSERT_EQUAL_INT((int)VadEvent::None, (int)vad.update(100, 800));  // 800-400=400 < 500
  TEST_ASSERT_TRUE(vad.active());
  TEST_ASSERT_EQUAL_INT((int)VadEvent::Stopped, (int)vad.update(100, 900));  // 900-400=500 >= 500
}

void test_vad_quiet_start_stays_idle() {
  Vad vad(1000, 500);
  TEST_ASSERT_EQUAL_INT((int)VadEvent::None, (int)vad.update(0, 0));
  TEST_ASSERT_EQUAL_INT((int)VadEvent::None, (int)vad.update(0, 1000));
  TEST_ASSERT_FALSE(vad.active());
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_vad_starts_on_loud);
  RUN_TEST(test_vad_stops_after_hangover);
  RUN_TEST(test_vad_hangover_resets_on_new_loud);
  RUN_TEST(test_vad_quiet_start_stays_idle);
  return UNITY_END();
}
