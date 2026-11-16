// events: the recent-event log. The tests share the log, so they run in order.

#include <Arduino.h>
#include <time.h>
#include <unity.h>

#include "m5ui.h"

namespace events = m5ui::events;

void setUp() {}
void tearDown() {}

// The part after the time stamp.
static String message(size_t i) {
  const String& line = events::at(i);
  return line.substring(line.indexOf(' ') + 1);
}

void test_starts_empty() {
  TEST_ASSERT_EQUAL_UINT32(0, events::count());
}

void test_formats_like_printf() {
  events::add("%d-%s", 7, "x");
  TEST_ASSERT_EQUAL_UINT32(1, events::count());
  TEST_ASSERT_EQUAL_STRING("7-x", message(0).c_str());
}

void test_stamps_seconds_since_boot_without_a_clock() {
  if (time(nullptr) > 1735689600) TEST_IGNORE_MESSAGE("the clock is set");
  const String& line = events::at(0);
  TEST_ASSERT_EQUAL_INT('+', line[0]);
  int space = line.indexOf(' ');
  TEST_ASSERT_TRUE(space >= 3);  // "+Ns"
  TEST_ASSERT_EQUAL_INT('s', line[space - 1]);
}

void test_newest_first() {
  events::add("older");
  events::add("newer");
  TEST_ASSERT_EQUAL_STRING("newer", message(0).c_str());
  TEST_ASSERT_EQUAL_STRING("older", message(1).c_str());
}

void test_keeps_the_last_entries() {
  for (int i = 0; i < 20; i++) events::add("e%d", i);
  TEST_ASSERT_EQUAL_UINT32(events::KEEP, events::count());
  TEST_ASSERT_EQUAL_STRING("e19", message(0).c_str());
  TEST_ASSERT_EQUAL_STRING("e8", message(events::KEEP - 1).c_str());
}

void test_cuts_long_messages() {
  String longText;
  for (int i = 0; i < 200; i++) longText += 'x';
  events::add("%s", longText.c_str());
  TEST_ASSERT_EQUAL_UINT32(159, message(0).length());
}

void setup() {
  delay(2000);  // lets the USB serial come up so the first results are not lost
  UNITY_BEGIN();
  RUN_TEST(test_starts_empty);
  RUN_TEST(test_formats_like_printf);
  RUN_TEST(test_stamps_seconds_since_boot_without_a_clock);
  RUN_TEST(test_newest_first);
  RUN_TEST(test_keeps_the_last_entries);
  RUN_TEST(test_cuts_long_messages);
  UNITY_END();
}

void loop() {}
