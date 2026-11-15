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
// Allocates a 16-bit sprite, in PSRAM if there is any. False if out of memory.
bool makeSprite(M5Canvas& c, int w, int h);

}  // namespace detail
}  // namespace m5ui
