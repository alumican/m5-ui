#include "m5ui.h"

namespace m5ui {

namespace ease {
float linear(float t) { return t; }
float inOut(float t) { return t * t * (3 - 2 * t); }
float inQuad(float t) { return t * t; }
float outQuad(float t) { return t * (2 - t); }
float inOutQuad(float t) { return t < 0.5f ? 2 * t * t : 1 - 2 * (1 - t) * (1 - t); }
float inCubic(float t) { return t * t * t; }
float outCubic(float t) {
  float u = 1 - t;
  return 1 - u * u * u;
}
float inOutCubic(float t) {
  if (t < 0.5f) return 4 * t * t * t;
  float u = 1 - t;
  return 1 - 4 * u * u * u;
}
}  // namespace ease

static int lerp(int a, int b, float t) { return a + (int)((b - a) * t + (b >= a ? 0.5f : -0.5f)); }

uint16_t mix565(uint16_t a, uint16_t b, float t) {
  t = t < 0 ? 0 : t > 1 ? 1 : t;
  return lerp(a >> 11, b >> 11, t) << 11 | lerp((a >> 5) & 63, (b >> 5) & 63, t) << 5 | lerp(a & 31, b & 31, t);
}

uint32_t mix888(uint32_t a, uint32_t b, float t) {
  t = t < 0 ? 0 : t > 1 ? 1 : t;
  return (uint32_t)lerp((a >> 16) & 255, (b >> 16) & 255, t) << 16 | lerp((a >> 8) & 255, (b >> 8) & 255, t) << 8 |
         lerp(a & 255, b & 255, t);
}

void Tween::start(uint32_t length, Ease c, uint32_t now) {
  startedAt = now;
  ms = length;
  curve = c;
}

float Tween::value(uint32_t now) const {
  if (!ms || now - startedAt >= ms) return 1;
  float t = (now - startedAt) / (float)ms;
  return curve ? curve(t) : t;
}

}  // namespace m5ui
