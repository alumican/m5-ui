#include <algorithm>

#include "m5ui_internal.h"

namespace m5ui {

static const uint16_t KEY = 0x0821;  // transparent: the corners outside the rounded square

// ---- Icons ----

// Microphone in color: capsule, cradle, stem and base, within about 0.1 - 0.85 of the size.
static void drawMic(LovyanGFX& g, int s, uint16_t color) {
  int cx = s / 2, t = std::max(2, s / 12), gap = std::max(2, s / 14);
  int cw = std::max(4, s * 8 / 25) | 1, ch = s * 9 / 20, top = s / 10;
  g.fillRoundRect(cx - cw / 2, top, cw, ch, cw / 2, color);
  int ay = top + ch - cw / 2 - 1, r0 = cw / 2 + gap, r1 = r0 + t;
  g.fillArc(cx, ay, r1, r0, 0, 180, color);
  g.fillRect(cx - r1, ay - t, t + 1, t + 1, color);  // arc tips pointing up
  g.fillRect(cx + r0, ay - t, t + 1, t + 1, color);
  int stemTop = ay + r1, baseY = std::max(stemTop + t, s * 17 / 20);
  g.fillRect(cx - t / 2, stemTop, t, baseY - stemTop, color);
  g.fillRect(cx - r0, baseY, r0 * 2 + 1, t, color);
}

static void micIcon(LovyanGFX& g, int s, bool on) {
  drawMic(g, s, theme.iconOn);
  if (on) g.fillCircle(s * 5 / 6, s / 6, std::max(2, s / 9), theme.iconAlert);
}

static void micOffIcon(LovyanGFX& g, int s, bool) {
  drawMic(g, s, theme.iconOff);
  int m = s / 6, w = std::max(1, s / 20);
  g.drawWideLine(m, m, s - m, s - m, w + 2, theme.badgeBg);  // gap around the slash
  g.drawWideLine(m, m, s - m, s - m, w, theme.iconOff);
}

namespace icons {
const Icon mic = {micIcon, true};
const Icon micOff = {micOffIcon, false};
}  // namespace icons

// ---- Badge ----

bool Badge::phaseAt(uint32_t now) const {
  return !icon || !icon->blinks || !timing.badgeBlinkMs || (now / timing.badgeBlinkMs) % 2 == 0;
}

void Badge::set(const Icon& i) {
  if (icon == &i) return;
  icon = &i;
  phase = phaseAt(millis());
  draw();
}

void Badge::hide() {
  if (!icon) return;
  icon = nullptr;
  detail::reserved = {0, 0, 0, 0};
  const Metrics& m = metrics;
  M5.Display.fillRect(m.badgeX, m.badgeY, m.badgeSize, m.badgeSize, theme.screen);
}

void Badge::show() {
  if (icon) draw();
}

void Badge::update() {
  if (!icon || !icon->blinks) return;
  bool p = phaseAt(millis());
  if (p == phase) return;
  phase = p;
  draw();
}

void Badge::draw() {
  const Metrics& m = metrics;
  int s = m.badgeSize;
  if (s <= 0) return;
  detail::reserved = {m.badgeX, m.badgeY, s, s};
  if (canvas.width() != s || !canvas.getBuffer()) {  // small: internal RAM
    if (canvas.getBuffer()) canvas.deleteSprite();
    canvas.setColorDepth(16);
    canvas.setPsram(false);
    if (!canvas.createSprite(s, s)) return;
  }
  canvas.fillScreen(KEY);
  canvas.fillRoundRect(0, 0, s, s, s / 5, theme.badgeBg);
  icon->draw(canvas, s, phase);
  canvas.pushSprite(&M5.Display, m.badgeX, m.badgeY, KEY);
}

}  // namespace m5ui
