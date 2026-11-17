// Breadboard places, positions and projection, and color names.

#include <Arduino.h>
#include <unity.h>

#include "m5ui_internal.h"

using m5ui::detail::Camera;
using m5ui::detail::parsePlace;
using m5ui::detail::Place;
using m5ui::detail::placePosition;

void setUp() {}
void tearDown() {}

static void assertPlace(const char* s, int type, int a, int b) {
  Place p;
  TEST_ASSERT_TRUE_MESSAGE(parsePlace(s, p), s);
  TEST_ASSERT_EQUAL_INT_MESSAGE(type, p.type, s);
  TEST_ASSERT_EQUAL_INT_MESSAGE(a, p.a, s);
  TEST_ASSERT_EQUAL_INT_MESSAGE(b, p.b, s);
}

void test_parse_holes() {
  assertPlace("10e", 1, 10, 4);
  assertPlace("1a", 1, 1, 0);
  assertPlace("30j", 1, 30, 9);
  assertPlace("15C", 1, 15, 2);
}

void test_parse_rails_and_pins() {
  assertPlace("+:16", 2, 16, 1);
  assertPlace("-:16", 2, 16, -1);
  assertPlace("+a:3", 2, 3, 2);
  assertPlace("-a:3", 2, 3, -2);
  assertPlace("pin:11", 3, 11, 0);
}

void test_parse_rejects() {
  Place p;
  for (const char* s : {"", "e", "10", "10k", "0a", "a10", "10ee", "+16", "+b:16", "+:0", "pin:", "pin:0", "pin:x"})
    TEST_ASSERT_FALSE_MESSAGE(parsePlace(s, p), s);
}

static void assertPosition(const char* s, float x, float y, float z) {
  Place p;
  float px, py, pz;
  TEST_ASSERT_TRUE_MESSAGE(parsePlace(s, p) && placePosition(p, 30, 40, px, py, pz), s);
  TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, x, px, s);
  TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, y, py, s);
  TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, z, pz, s);
}

void test_positions() {
  assertPosition("1a", -14.5f, -5.5f, 0);
  assertPosition("10e", -5.5f, -1.5f, 0);
  assertPosition("10f", -5.5f, 1.5f, 0);  // three pitches across the groove
  assertPosition("30j", 14.5f, 5.5f, 0);
  assertPosition("-:16", 0.5f, 7.5f, 0);
  assertPosition("+:16", 0.5f, 8.5f, 0);
  assertPosition("-a:16", 0.5f, -7.5f, 0);
  float hx = m5ui::detail::headerX(30);
  assertPosition("pin:1", hx - 0.5f, 9.5f, 1.9f);   // far end, odd pins on the left
  assertPosition("pin:2", hx + 0.5f, 9.5f, 1.9f);
  assertPosition("pin:40", hx + 0.5f, -9.5f, 1.9f);
}

void test_positions_out_of_range() {
  Place p;
  float x, y, z;
  TEST_ASSERT_TRUE(parsePlace("31a", p));
  TEST_ASSERT_FALSE(placePosition(p, 30, 40, x, y, z));
  TEST_ASSERT_TRUE(parsePlace("pin:41", p));
  TEST_ASSERT_FALSE(placePosition(p, 30, 40, x, y, z));
}

void test_camera_center_and_directions() {
  Camera c;
  c.scale = 10, c.cx = 160, c.cy = 70, c.pitch = 90, c.yaw = 0;
  c.prepare();
  float x, y, d;
  c.project(0, 0, 0, x, y, d);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 160, x);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 70, y);
  c.project(1, 0, 0, x, y, d);  // seen from above: x goes right
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 170, x);
  c.project(0, 1, 0, x, y, d);  // the far side goes up
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 60, y);
  float dLow, dHigh;
  c.project(0, 0, 0, x, y, dLow);
  c.project(0, 0, 1, x, y, dHigh);  // higher is nearer
  TEST_ASSERT_TRUE(dHigh > dLow);
}

void test_camera_turns() {
  Camera c;
  c.scale = 10, c.cx = 0, c.cy = 0, c.pitch = 90, c.yaw = 90;
  c.prepare();
  float x, y, d;
  c.project(1, 0, 0, x, y, d);  // a quarter turn: x now points to the far side (up)
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0, x);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -10, y);
}

void test_camera_side_view() {
  Camera c;
  c.scale = 10, c.cx = 0, c.cy = 0, c.pitch = 0, c.yaw = 0;
  c.prepare();
  float x, y, d, dNear, dFar;
  c.project(0, 0, 1, x, y, d);  // level view: up is up
  TEST_ASSERT_TRUE(y < 0);
  c.project(0, -1, 0, x, y, dNear);
  c.project(0, 1, 0, x, y, dFar);
  TEST_ASSERT_TRUE(dNear > dFar);
}

void test_color_names() {
  TEST_ASSERT_EQUAL_HEX16(m5ui::theme.bands[4], m5ui::colorByName("yellow"));
  TEST_ASSERT_EQUAL_HEX16(m5ui::theme.bands[0], m5ui::colorByName("Black"));
  TEST_ASSERT_EQUAL_HEX16(m5ui::theme.bands[7], m5ui::colorByName("purple"));
  TEST_ASSERT_EQUAL_HEX16(m5ui::theme.bands[8], m5ui::colorByName("grey"));
  TEST_ASSERT_EQUAL_HEX16(0x1234, m5ui::colorByName("teal", 0x1234));
}

void setup() {
  delay(2000);  // lets the USB serial come up so the first results are not lost
  UNITY_BEGIN();
  RUN_TEST(test_parse_holes);
  RUN_TEST(test_parse_rails_and_pins);
  RUN_TEST(test_parse_rejects);
  RUN_TEST(test_positions);
  RUN_TEST(test_positions_out_of_range);
  RUN_TEST(test_camera_center_and_directions);
  RUN_TEST(test_camera_turns);
  RUN_TEST(test_camera_side_view);
  RUN_TEST(test_color_names);
  UNITY_END();
}

void loop() {}
