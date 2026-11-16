// Shared helpers: text wrapping and cutting, ink, light colors, UTF-8 and damage tracking.

#include <Arduino.h>
#include <unity.h>

#include "m5ui_internal.h"

using namespace m5ui;
using namespace m5ui::detail;

static M5Canvas canvas;  // only measures text
static std::vector<String> lines;
static const int CHAR_W = 6;  // fonts::Font0

void setUp() {
  canvas.setFont(&fonts::Font0);
  canvas.setTextSize(1);
  lines.clear();
}

void tearDown() {}

static void assertLines(const char* const* expected, size_t n) {
  TEST_ASSERT_EQUAL_UINT32(n, lines.size());
  for (size_t i = 0; i < n; i++) TEST_ASSERT_EQUAL_STRING(expected[i], lines[i].c_str());
}

void test_font_width() {
  TEST_ASSERT_EQUAL_INT(6 * CHAR_W, canvas.textWidth("abc de"));
}

void test_wrap_at_spaces() {
  wrap(canvas, "hello world", 6 * CHAR_W, lines);
  const char* expected[] = {"hello", "world"};
  assertLines(expected, 2);
}

void test_wrap_keeps_text_that_fits() {
  wrap(canvas, "hello world", 100 * CHAR_W, lines);
  const char* expected[] = {"hello world"};
  assertLines(expected, 1);
}

void test_wrap_breaks_long_words() {
  wrap(canvas, "abcdefghij", 4 * CHAR_W, lines);
  const char* expected[] = {"abcd", "efgh", "ij"};
  assertLines(expected, 3);
}

void test_wrap_drops_spaces_at_breaks() {
  wrap(canvas, "ab   cd", 3 * CHAR_W, lines);
  const char* expected[] = {"ab", "cd"};
  assertLines(expected, 2);
}

void test_wrap_at_newlines() {
  wrap(canvas, "a\n\nb", 100, lines);
  const char* expected[] = {"a", "", "b"};
  assertLines(expected, 3);
}

void test_wrap_empty_text_is_one_empty_line() {
  wrap(canvas, "", 100, lines);
  const char* expected[] = {""};
  assertLines(expected, 1);
}

void test_wrap_appends() {
  lines.push_back("kept");
  wrap(canvas, "new", 100, lines);
  const char* expected[] = {"kept", "new"};
  assertLines(expected, 2);
}

void test_wrap_never_splits_utf8_characters() {
  canvas.setFont(&fonts::efontJA_12);
  const String text = "\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A";  // five kana, no spaces
  const int w = canvas.textWidth("\xE3\x81\x82\xE3\x81\x84");                           // two of them
  TEST_ASSERT_TRUE(w > 0);
  wrap(canvas, text, w, lines);
  TEST_ASSERT_TRUE(lines.size() > 1);
  String joined;
  for (const String& line : lines) {
    TEST_ASSERT_TRUE(line.length() > 0);
    TEST_ASSERT_TRUE(((uint8_t)line[0] & 0xC0) != 0x80);  // starts at a character
    TEST_ASSERT_TRUE(canvas.textWidth(line) <= w);
    joined += line;
  }
  TEST_ASSERT_EQUAL_STRING(text.c_str(), joined.c_str());
}

void test_fit_keeps_text_that_fits() {
  TEST_ASSERT_EQUAL_STRING("hello", fit(canvas, "hello", 5 * CHAR_W).c_str());
}

void test_fit_cuts_with_dots() {
  TEST_ASSERT_EQUAL_STRING("hel...", fit(canvas, "hello world", 6 * CHAR_W).c_str());
}

void test_fit_never_splits_utf8_characters() {
  canvas.setFont(&fonts::efontJA_12);
  const String text = "\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A";
  const int w = canvas.textWidth("\xE3\x81\x82\xE3\x81\x84...");
  String cut = fit(canvas, text, w);
  TEST_ASSERT_TRUE(cut.endsWith("..."));
  TEST_ASSERT_TRUE(canvas.textWidth(cut) <= w);
  TEST_ASSERT_EQUAL_INT(0, (int)(cut.length() - 3) % 3);  // whole 3-byte kana before the dots
  TEST_ASSERT_TRUE(text.startsWith(cut.substring(0, cut.length() - 3)));
}

void test_char_len() {
  TEST_ASSERT_EQUAL_INT(1, charLen('a'));
  TEST_ASSERT_EQUAL_INT(2, charLen(0xC3));
  TEST_ASSERT_EQUAL_INT(3, charLen(0xE3));
  TEST_ASSERT_EQUAL_INT(4, charLen(0xF0));
  TEST_ASSERT_EQUAL_INT(1, charLen(0x80));  // stray continuation byte
}

void test_ink_reads_on_its_background() {
  TEST_ASSERT_EQUAL_HEX16(TFT_BLACK, ink(TFT_WHITE));
  TEST_ASSERT_EQUAL_HEX16(TFT_BLACK, ink(0xFFE0));  // yellow
  TEST_ASSERT_EQUAL_HEX16(TFT_BLACK, ink(0x07E0));  // green
  TEST_ASSERT_EQUAL_HEX16(TFT_WHITE, ink(TFT_BLACK));
  TEST_ASSERT_EQUAL_HEX16(TFT_WHITE, ink(0x001F));  // blue
  TEST_ASSERT_EQUAL_HEX16(TFT_WHITE, ink(0xF800));  // red
}

void test_light_colors_follow_the_theme() {
  Theme saved = theme;
  theme.ok = 0x1111, theme.wait = 0x2222, theme.ng = 0x3333, theme.off = 0x4444;
  TEST_ASSERT_EQUAL_HEX16(0x1111, lightColor(Light::Ok));
  TEST_ASSERT_EQUAL_HEX16(0x2222, lightColor(Light::Wait));
  TEST_ASSERT_EQUAL_HEX16(0x3333, lightColor(Light::Ng));
  TEST_ASSERT_EQUAL_HEX16(0x4444, lightColor(Light::Off));
  theme = saved;
}

void test_damage_is_seen_once() {
  uint32_t seen = damageSeq;
  Area a = {0, 0, 10, 10};
  TEST_ASSERT_FALSE(damaged(seen, a));  // nothing new
  damage = {5, 5, 10, 10};
  damageSeq++;
  TEST_ASSERT_TRUE(damaged(seen, a));
  TEST_ASSERT_FALSE(damaged(seen, a));
}

void test_damage_elsewhere_is_ignored() {
  uint32_t seen = damageSeq;
  Area a = {0, 0, 10, 10};
  damage = {10, 0, 5, 5};  // touches the edge only
  damageSeq++;
  TEST_ASSERT_FALSE(damaged(seen, a));
  damage = {0, 0, 0, 0};  // empty
  damageSeq++;
  TEST_ASSERT_FALSE(damaged(seen, a));
}

void setup() {
  delay(2000);  // lets the USB serial come up so the first results are not lost
  canvas.setPsram(false);
  canvas.createSprite(1, 1);
  UNITY_BEGIN();
  RUN_TEST(test_font_width);
  RUN_TEST(test_wrap_at_spaces);
  RUN_TEST(test_wrap_keeps_text_that_fits);
  RUN_TEST(test_wrap_breaks_long_words);
  RUN_TEST(test_wrap_drops_spaces_at_breaks);
  RUN_TEST(test_wrap_at_newlines);
  RUN_TEST(test_wrap_empty_text_is_one_empty_line);
  RUN_TEST(test_wrap_appends);
  RUN_TEST(test_wrap_never_splits_utf8_characters);
  RUN_TEST(test_fit_keeps_text_that_fits);
  RUN_TEST(test_fit_cuts_with_dots);
  RUN_TEST(test_fit_never_splits_utf8_characters);
  RUN_TEST(test_char_len);
  RUN_TEST(test_ink_reads_on_its_background);
  RUN_TEST(test_light_colors_follow_the_theme);
  RUN_TEST(test_damage_is_seen_once);
  RUN_TEST(test_damage_elsewhere_is_ignored);
  UNITY_END();
}

void loop() {}
