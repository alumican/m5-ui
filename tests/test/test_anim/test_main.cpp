// Easing curves, color mixing and Tween.

#include <Arduino.h>
#include <unity.h>

#include "m5ui.h"

using namespace m5ui;

static const Ease curves[] = {ease::linear,  ease::inOut,    ease::inQuad,  ease::outQuad,
                              ease::inOutQuad, ease::inCubic, ease::outCubic, ease::inOutCubic};
static const int CURVES = sizeof curves / sizeof curves[0];

void setUp() {}
void tearDown() {}

void test_curves_start_at_0_and_end_at_1() {
  for (int i = 0; i < CURVES; i++) {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0, curves[i](0));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1, curves[i](1));
  }
}

void test_curves_never_go_back() {
  for (int i = 0; i < CURVES; i++) {
    float last = curves[i](0);
    for (int k = 1; k <= 100; k++) {
      float v = curves[i](k / 100.0f);
      TEST_ASSERT_TRUE(v >= last - 1e-6f);
      last = v;
    }
  }
}

void test_curve_values() {
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, ease::inOut(0.5f));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.15625f, ease::inOut(0.25f));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.25f, ease::inQuad(0.5f));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.75f, ease::outQuad(0.5f));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, ease::inOutQuad(0.5f));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.125f, ease::inCubic(0.5f));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.875f, ease::outCubic(0.5f));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, ease::inOutCubic(0.5f));
}

void test_mix565_ends_and_clamps() {
  TEST_ASSERT_EQUAL_HEX16(0x1234, mix565(0x1234, 0xABCD, 0));
  TEST_ASSERT_EQUAL_HEX16(0xABCD, mix565(0x1234, 0xABCD, 1));
  TEST_ASSERT_EQUAL_HEX16(0x1234, mix565(0x1234, 0xABCD, -1));
  TEST_ASSERT_EQUAL_HEX16(0xABCD, mix565(0x1234, 0xABCD, 2));
}

void test_mix565_mixes_each_channel() {
  TEST_ASSERT_EQUAL_HEX16(0x8410, mix565(TFT_BLACK, TFT_WHITE, 0.5f));
  TEST_ASSERT_EQUAL_HEX16(0x7BEF, mix565(TFT_WHITE, TFT_BLACK, 0.5f));
  TEST_ASSERT_EQUAL_HEX16(0xB808, mix565(0xF800, 0x001F, 0.25f));  // red to blue
}

void test_mix888() {
  TEST_ASSERT_EQUAL_HEX32(0x808080, mix888(0x000000, 0xFFFFFF, 0.5f));
  TEST_ASSERT_EQUAL_HEX32(0x282828, mix888(0x102030, 0x403020, 0.5f));
  TEST_ASSERT_EQUAL_HEX32(0xFF0000, mix888(0xFF0000, 0x0000FF, -1));
  TEST_ASSERT_EQUAL_HEX32(0x0000FF, mix888(0xFF0000, 0x0000FF, 2));
}

void test_mix_float() {
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 12.5f, mix(10, 20, 0.25f));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 17.5f, mix(20, 10, 0.25f));
}

void test_tween_before_start_is_done() {
  Tween t;
  TEST_ASSERT_EQUAL_FLOAT(1, t.value(12345));
  TEST_ASSERT_FALSE(t.running(12345));
}

void test_tween_over_time() {
  Tween t;
  t.start(100, ease::linear, 1000);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0, t.value(1000));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, t.value(1050));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1, t.value(1100));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1, t.value(5000));
  TEST_ASSERT_TRUE(t.running(1000));
  TEST_ASSERT_TRUE(t.running(1099));
  TEST_ASSERT_FALSE(t.running(1100));
}

void test_tween_applies_its_curve() {
  Tween t;
  t.start(100, ease::inQuad, 0);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.25f, t.value(50));
  t.start(100, nullptr, 0);  // no curve: linear
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, t.value(50));
}

void test_tween_across_millis_wraparound() {
  Tween t;
  t.start(100, ease::linear, 0xFFFFFFCEu);  // 50 ms before the counter wraps
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, t.value(0));
  TEST_ASSERT_TRUE(t.running(0));
  TEST_ASSERT_FALSE(t.running(50));
}

void test_tween_of_zero_length_is_done() {
  Tween t;
  t.start(0, ease::linear, 1000);
  TEST_ASSERT_EQUAL_FLOAT(1, t.value(1000));
  TEST_ASSERT_FALSE(t.running(1000));
}

void setup() {
  delay(2000);  // lets the USB serial come up so the first results are not lost
  UNITY_BEGIN();
  RUN_TEST(test_curves_start_at_0_and_end_at_1);
  RUN_TEST(test_curves_never_go_back);
  RUN_TEST(test_curve_values);
  RUN_TEST(test_mix565_ends_and_clamps);
  RUN_TEST(test_mix565_mixes_each_channel);
  RUN_TEST(test_mix888);
  RUN_TEST(test_mix_float);
  RUN_TEST(test_tween_before_start_is_done);
  RUN_TEST(test_tween_over_time);
  RUN_TEST(test_tween_applies_its_curve);
  RUN_TEST(test_tween_across_millis_wraparound);
  RUN_TEST(test_tween_of_zero_length_is_done);
  UNITY_END();
}

void loop() {}
