#pragma once
// Shared by the m5-ui sources. Not part of the public API.

#include "m5ui.h"

namespace m5ui {
namespace detail {

uint16_t lightColor(Light light);
// Black or white, whichever reads better on bg.
uint16_t ink(uint16_t bg);
// Splits text into lines no wider than w in g's font: at spaces, or between characters for long words and text
// without spaces (e.g. Japanese). '\n' starts a new line. Appends to out.
void wrap(LovyanGFX& g, const String& text, int w, std::vector<String>& out);
// text, cut with "..." to fit w.
String fit(LovyanGFX& g, const String& text, int w);
// Bytes in the UTF-8 character that starts with c.
int charLen(uint8_t c);
// Resistor 4-band code: indexes into theme.bands for two digits, multiplier and tolerance (-1: no band). False if
// the value cannot be coded (not above 0, below 0.1 ohm, or beyond white x 10^9).
bool colorCode(float ohms, float tolerancePercent, int out[4]);
struct Area {
  int x, y, w, h;
};
// Areas kept clear while shown: a Subtitle's band. w 0 when not shown.
enum { RESERVED_SUBTITLE, RESERVED };
extern Area reserved[RESERVED];
// A visible Badge: drawn into every sprite pushed over it, so it neither flickers nor gets wiped.
extern M5Canvas* badge;
extern Area badgeArea;
const uint16_t BADGE_KEY = 0x0821;  // transparent color of the Badge sprite
// Areas left stale (a Badge hidden); components over them redraw. seq counts the changes.
extern Area damage;
extern uint32_t damageSeq;
// True once if damage changed since seen and touches a.
bool damaged(uint32_t& seen, Area a);
// Pushes c (placed at x, y) to the display, only inside a, leaving out the reserved areas except skip, with a
// visible Badge drawn in. Resets the display clip rect.
void push(M5Canvas& c, int x, int y, Area a, int skip = -1);
void push(M5Canvas& c, int x, int y, int skip = -1);
// Allocates a 16-bit sprite, in PSRAM if there is any. False if out of memory.
bool makeSprite(M5Canvas& c, int w, int h);

}  // namespace detail
}  // namespace m5ui
