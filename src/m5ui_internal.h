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
// Breadboard places. type: 0 none, 1 hole (a: column, b: row 0-9 for a-j), 2 rail (a: column, b: +1 / -1 past
// row j, +2 / -2 past row a), 3 header pin (a: physical number).
struct Place {
  uint8_t type;
  int16_t a;
  int8_t b;
};
// "10e", "+:16", "-a:16", "pin:11". False if not one of these.
bool parsePlace(const String& s, Place& out);
// Position in hole pitches: x along the columns (0 at the middle), y across (row j side positive), z up (0 on
// the breadboard). columns: breadboard columns. pins: header pins.
bool placePosition(const Place& p, int columns, int pins, float& x, float& y, float& z);
// x of the header's middle, and the breadboard's half width and depth.
float headerX(int columns);
float boardHalfW(int columns);
const float BOARD_HALF_D = 9.5f;

// Projection for the breadboard view: turns around (tx, ty, tz) by yaw, looks down by pitch (degrees), scale
// px per pitch, mild perspective. depth grows toward the viewer.
struct Camera {
  float yaw = 0, pitch = 55, scale = 8, tx = 0, ty = 0, tz = 0, cx = 160, cy = 70;
  void prepare();
  void project(float x, float y, float z, float& sx, float& sy, float& depth) const;
  float sinYaw = 0, cosYaw = 1, sinPitch = 0, cosPitch = 1;
};

struct Area {
  int x, y, w, h;
};
// Areas kept clear while shown: a Subtitle's band. w 0 when not shown.
enum { RESERVED_SUBTITLE, RESERVED };
extern Area reserved[RESERVED];
// Things that float over the rest: a round Face and a visible Badge, in drawing order. Each is drawn into every
// sprite pushed under it (transparent where KEY), so neither flickers nor gets wiped.
const uint16_t KEY = 0x0821;  // transparent color of floating sprites
enum { OVERLAY_FACE, OVERLAY_BADGE, OVERLAYS };
struct Overlay {
  M5Canvas* canvas;  // nullptr when not shown
  Area area;
};
extern Overlay overlays[OVERLAYS];
// Areas left stale (a Badge hidden); components over them redraw. seq counts the changes.
extern Area damage;
extern uint32_t damageSeq;
// True once if damage changed since seen and touches a.
bool damaged(uint32_t& seen, Area a);
// Pushes c (placed at x, y) to the display, only inside a, leaving out the reserved areas except skip, with the
// overlays drawn in except self. key >= 0: c's pixels of that color are left out. Resets the display clip rect.
void push(M5Canvas& c, int x, int y, Area a, int skip = -1, int self = -1, int32_t key = -1);
void push(M5Canvas& c, int x, int y, int skip = -1);
// Allocates a 16-bit sprite, in PSRAM if there is any. False if out of memory.
bool makeSprite(M5Canvas& c, int w, int h);

}  // namespace detail
}  // namespace m5ui
