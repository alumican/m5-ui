#include "m5ui_internal.h"

#include <algorithm>

namespace m5ui {

Theme theme;
Metrics metrics;
Timing timing;
Words words;

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

Area reserved[RESERVED] = {};
M5Canvas* badge = nullptr;
Area badgeArea = {0, 0, 0, 0};
Area damage = {0, 0, 0, 0};
uint32_t damageSeq = 0;

static bool overlaps(const Area& a, const Area& b) {
  return a.w > 0 && b.w > 0 && a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

bool damaged(uint32_t& seen, Area a) {
  if (seen == damageSeq) return false;
  seen = damageSeq;
  return overlaps(damage, a);
}

void push(M5Canvas& c, int x, int y, int skip) { push(c, x, y, {x, y, c.width(), c.height()}, skip); }

void push(M5Canvas& c, int x, int y, Area a, int skip) {
  int x0 = std::max(a.x, x), y0 = std::max(a.y, y);
  int x1 = std::min(a.x + a.w, x + c.width()), y1 = std::min(a.y + a.h, y + c.height());
  if (x0 >= x1 || y0 >= y1) return;
  if (badge && overlaps(badgeArea, {x0, y0, x1 - x0, y1 - y0}))
    badge->pushSprite(&c, badgeArea.x - x, badgeArea.y - y, BADGE_KEY);
  // Horizontal bands cut at the reserved edges; in each band, push the gaps between the reserved spans.
  int cuts[2 + RESERVED * 2], n = 0;
  cuts[n++] = y0;
  cuts[n++] = y1;
  for (int i = 0; i < RESERVED; i++) {
    const Area& r = reserved[i];
    if (i == skip || r.w <= 0 || r.h <= 0) continue;
    if (r.y > y0 && r.y < y1) cuts[n++] = r.y;
    if (r.y + r.h > y0 && r.y + r.h < y1) cuts[n++] = r.y + r.h;
  }
  std::sort(cuts, cuts + n);
  auto& d = M5.Display;
  for (int k = 0; k + 1 < n; k++) {
    int ya = cuts[k], yb = cuts[k + 1];
    if (ya >= yb) continue;
    int spans[RESERVED + 1][2], m = 0;  // one spare keeps the compiler sure of the bounds
    for (int i = 0; i < RESERVED; i++) {
      const Area& r = reserved[i];
      if (i == skip || r.w <= 0 || r.h <= 0 || r.y > ya || r.y + r.h < yb) continue;
      int s0 = std::max(r.x, x0), s1 = std::min(r.x + r.w, x1);
      if (s0 < s1) spans[m][0] = s0, spans[m][1] = s1, m++;
    }
    for (int j = 1; j < m; j++)  // by start
      for (int q = j; q > 0 && spans[q][0] < spans[q - 1][0]; q--) std::swap(spans[q], spans[q - 1]);
    int gx = x0;
    for (int j = 0; j <= m; j++) {
      int end = j < m ? spans[j][0] : x1;
      if (end > gx) {
        d.setClipRect(gx, ya, end - gx, yb - ya);
        c.pushSprite(&d, x, y);
      }
      if (j < m) gx = std::max(gx, spans[j][1]);
    }
  }
  d.clearClipRect();
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
