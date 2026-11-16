// Resistor color code for Circuit::setResistor().

#include <Arduino.h>
#include <unity.h>

#include "m5ui_internal.h"

using m5ui::detail::colorCode;

// Indexes into theme.bands.
namespace band {
enum { BLACK, BROWN, RED, ORANGE, YELLOW, GREEN, BLUE, VIOLET, GRAY, WHITE, GOLD, SILVER, NONE = -1 };
}

static int code[4];

void setUp() {
  for (int& c : code) c = 99;
}

void tearDown() {}

static void assertCode(float ohms, float tolerance, int a, int b, int multiplier, int tol) {
  TEST_ASSERT_TRUE(colorCode(ohms, tolerance, code));
  int expected[4] = {a, b, multiplier, tol};
  TEST_ASSERT_EQUAL_INT_ARRAY(expected, code, 4);
}

void test_common_values() {
  assertCode(330, 5, band::ORANGE, band::ORANGE, band::BROWN, band::GOLD);
  assertCode(220, 5, band::RED, band::RED, band::BROWN, band::GOLD);
  assertCode(1000, 5, band::BROWN, band::BLACK, band::RED, band::GOLD);
  assertCode(4700, 5, band::YELLOW, band::VIOLET, band::RED, band::GOLD);
  assertCode(10000, 5, band::BROWN, band::BLACK, band::ORANGE, band::GOLD);
  assertCode(68000, 5, band::BLUE, band::GRAY, band::ORANGE, band::GOLD);
  assertCode(1.5e6f, 5, band::BROWN, band::GREEN, band::GREEN, band::GOLD);
}

void test_powers_of_ten() {
  assertCode(1, 5, band::BROWN, band::BLACK, band::GOLD, band::GOLD);
  assertCode(10, 5, band::BROWN, band::BLACK, band::BLACK, band::GOLD);
  assertCode(100, 5, band::BROWN, band::BLACK, band::BROWN, band::GOLD);
  assertCode(1e6f, 5, band::BROWN, band::BLACK, band::GREEN, band::GOLD);
}

void test_below_ten_ohms() {
  assertCode(4.7f, 5, band::YELLOW, band::VIOLET, band::GOLD, band::GOLD);
  assertCode(0.47f, 5, band::YELLOW, band::VIOLET, band::SILVER, band::GOLD);
  assertCode(0.1f, 5, band::BROWN, band::BLACK, band::SILVER, band::GOLD);
}

void test_rounds_to_two_digits() {
  assertCode(336, 5, band::ORANGE, band::YELLOW, band::BROWN, band::GOLD);
}

void test_largest_value() {
  assertCode(99e9f, 5, band::WHITE, band::WHITE, band::WHITE, band::GOLD);
}

void test_tolerance_bands() {
  assertCode(330, 1, band::ORANGE, band::ORANGE, band::BROWN, band::BROWN);
  assertCode(330, 2, band::ORANGE, band::ORANGE, band::BROWN, band::RED);
  assertCode(330, 0.5f, band::ORANGE, band::ORANGE, band::BROWN, band::GREEN);
  assertCode(330, 0.25f, band::ORANGE, band::ORANGE, band::BROWN, band::BLUE);
  assertCode(330, 0.1f, band::ORANGE, band::ORANGE, band::BROWN, band::VIOLET);
  assertCode(330, 0.05f, band::ORANGE, band::ORANGE, band::BROWN, band::GRAY);
  assertCode(330, 10, band::ORANGE, band::ORANGE, band::BROWN, band::SILVER);
  assertCode(330, 20, band::ORANGE, band::ORANGE, band::BROWN, band::NONE);
}

void test_values_that_cannot_be_coded() {
  TEST_ASSERT_FALSE(colorCode(0, 5, code));
  TEST_ASSERT_FALSE(colorCode(-5, 5, code));
  TEST_ASSERT_FALSE(colorCode(NAN, 5, code));
  TEST_ASSERT_FALSE(colorCode(0.01f, 5, code));
  TEST_ASSERT_FALSE(colorCode(1e11f, 5, code));
}

void setup() {
  delay(2000);  // lets the USB serial come up so the first results are not lost
  UNITY_BEGIN();
  RUN_TEST(test_common_values);
  RUN_TEST(test_powers_of_ten);
  RUN_TEST(test_below_ten_ohms);
  RUN_TEST(test_rounds_to_two_digits);
  RUN_TEST(test_largest_value);
  RUN_TEST(test_tolerance_bands);
  RUN_TEST(test_values_that_cannot_be_coded);
  UNITY_END();
}

void loop() {}
