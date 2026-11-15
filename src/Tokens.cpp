#include "m5ui_internal.h"

#include <algorithm>

namespace m5ui {

Theme theme;
Metrics metrics;
Timing timing;

namespace detail {

uint16_t lightColor(Light light) {
  return light == Light::Ok ? theme.ok : light == Light::Wait ? theme.wait : light == Light::Ng ? theme.ng : theme.off;
}

uint16_t ink(uint16_t bg) {
  uint32_t r = bg >> 11, g = (bg >> 5) & 63, b = bg & 31;
  return r * 2 * 299 + g * 587 + b * 2 * 114 > 31000 ? TFT_BLACK : TFT_WHITE;
}

int charLen(uint8_t c) {
  if (c < 0x80) return 1;
  if ((c & 0xE0) == 0xC0) return 2;
  if ((c & 0xF0) == 0xE0) return 3;
  if ((c & 0xF8) == 0xF0) return 4;
  return 1;
}

// One paragraph, text[from, to), without '\n'.
static void wrapParagraph(LovyanGFX& g, const String& text, int from, int to, int w, std::vector<String>& out) {
  const char* s = text.c_str();
  if (from >= to) {
    out.push_back(String());
    return;
  }
  int start = from;
  while (start < to) {
    int i = start, width = 0, brk = -1;  // brk: just after the last space on this line
    while (i < to) {
      int len = std::min(charLen(s[i]), to - i);
      char ch[5];
      memcpy(ch, s + i, len);
      ch[len] = 0;
      int cw = g.textWidth(ch);
      if (width + cw > w && i > start) break;
      width += cw;
      i += len;
      if (len == 1 && s[i - 1] == ' ') brk = i;
    }
    if (i < to && brk > start) i = brk;
    int end = i;
    while (end > start && s[end - 1] == ' ') end--;
    out.push_back(text.substring(start, end));
    while (i < to && s[i] == ' ') i++;
    start = i;
  }
}

void wrap(LovyanGFX& g, const String& text, int w, std::vector<String>& out) {
  int n = text.length(), start = 0;
  for (;;) {
    int end = text.indexOf('\n', start);
    if (end < 0) end = n;
    wrapParagraph(g, text, start, end, w, out);
    if (end >= n) break;
    start = end + 1;
  }
}

String fit(LovyanGFX& g, const String& text, int w) {
  if (g.textWidth(text) <= w) return text;
  String s = text;
  while (s.length()) {
    int n = s.length() - 1;
    while (n > 0 && ((uint8_t)s[n] & 0xC0) == 0x80) n--;  // back to the start of the last character
    s.remove(n);
    if (g.textWidth(s + "...") <= w) break;
  }
  return s + "...";
}

bool makeSprite(M5Canvas& c, int w, int h) {
  if (c.getBuffer() && c.width() == w && c.height() == h) return true;
  if (c.getBuffer()) c.deleteSprite();
  if (w <= 0 || h <= 0) return false;
  c.setColorDepth(16);
  c.setPsram(true);
  if (c.createSprite(w, h)) return true;
  c.setPsram(false);
  return c.createSprite(w, h) != nullptr;
}

}  // namespace detail
}  // namespace m5ui
