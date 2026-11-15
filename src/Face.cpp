#include <math.h>

#include <algorithm>

#include "m5ui_internal.h"

namespace m5ui {

static void mixKey(uint32_t& h, int v) { h = (h ^ (uint32_t)v) * 16777619u; }

uint32_t Face::rnd(uint32_t n) {
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  return n ? seed % n : 0;
}

bool Face::begin(int x, int y, int w, int h) {
  fx = x;
  fy = y;
  if (!colored) {
    fg = theme.face;
    bg = theme.faceBg;
    colored = true;
  }
  seed ^= millis() * 2654435761u + x * 31 + w;
  full = true;
  box[0] = box[1] = box[2] = box[3] = 0;
  return detail::makeSprite(canvas, w, h);
}

void Face::setLayout(const FaceLayout& l) {
  lay = l;
  full = true;
}

void Face::onDraw(Overlay o, int x, int y, int w, int h) {
  overlay = o;
  ox = x, oy = y, ow = w, oh = h;
  invalidate();
}

void Face::invalidate() { dirty = true; }

void Face::setEyes(Eyes e) { style = e; }
void Face::setMouth(float open) { mouth = open < 0 ? 0 : open > 1 ? 1 : open; }

void Face::setGaze(float x, float y) {
  autoGaze = false;
  tx = constrain(x, -1.0f, 1.0f);
  ty = constrain(y, -1.0f, 1.0f);
}

void Face::setAutoGaze(bool on) {
  autoGaze = on;
  nextGaze = millis();
}

void Face::setAutoBlink(bool on) {
  autoBlink = on;
  blinkUntil = 0;
}

void Face::setBreath(bool on) { breath = on; }

void Face::setColor(uint16_t c) {
  if (!colored) bg = theme.faceBg;
  colored = true;
  fg = c;
}

void Face::setBackground(uint16_t c) {
  if (!colored) fg = theme.face;
  colored = true;
  bg = c;
}

void Face::show() {
  full = true;
  update();
}

void Face::update() {
  if (!canvas.getBuffer()) return;
  const Metrics& m = metrics;
  const Timing& t = timing;
  uint32_t now = millis();
  if (detail::damaged(damageSeen, {fx, fy, canvas.width(), canvas.height()})) full = true;
  if (!full && now - drawnAt < t.faceFrameMs) return;

  // Time: blink and gaze.
  if (autoBlink) {
    if (!nextBlink) nextBlink = now + t.blinkEveryMs + rnd(t.blinkRandomMs);
    if ((int32_t)(now - nextBlink) >= 0) {
      blinkUntil = now + t.blinkClosedMs;
      nextBlink = blinkUntil + t.blinkEveryMs + rnd(t.blinkRandomMs);
    }
  }
  bool closed = (int32_t)(blinkUntil - now) > 0;
  if (autoGaze && (int32_t)(now - nextGaze) >= 0) {
    bool center = rnd(3) == 0;
    tx = center ? 0 : (int)rnd(201) / 100.0f - 1;
    ty = center ? 0 : ((int)rnd(201) / 100.0f - 1) * 0.6f;
    nextGaze = now + t.gazeEveryMs + rnd(t.gazeRandomMs);
  }
  float k = std::min(1.0f, (now - lastAt) / 120.0f);  // eyes move quickly but not at once
  lastAt = now;
  gx += (tx - gx) * k;
  gy += (ty - gy) * k;

  // Layout, scaled to the rect, with minimums for small faces.
  const FaceLayout& L = lay;
  const int W = canvas.width(), H = canvas.height();
  const float s = std::min(W / (float)L.width, H / (float)L.height);
  const float ox0 = (W - L.width * s) / 2, oy0 = (H - L.height * s) / 2;
  auto X = [&](int v) { return (int)lroundf(ox0 + v * s); };
  auto Y = [&](int v) { return (int)lroundf(oy0 + v * s); };
  auto px = [&](float v, int least) { return std::max(least, (int)lroundf(v * s)); };
  const int r = px(L.eyeR, m.faceMinEye), line = std::max<int>(m.faceMinLine, r / 2);
  const int reach = L.gaze ? px(L.gaze, 2) : 0, lift = breath && L.breath ? px(L.breath, 1) : 0;
  const int up = lift ? (int)lroundf(sinf(6.2831853f * (now % t.breathMs) / t.breathMs) * lift) : 0;
  float lookX = gx, lookY = gy;
  if (style == Eyes::Thinking) lookX = 0.5f, lookY = -1;  // looking up while thinking
  const int ex = (int)lroundf(lookX * reach), ey = (int)lroundf(lookY * reach);
  const int eyeX[2] = {X(L.leftEyeX), X(L.rightEyeX)}, eyeY[2] = {Y(L.leftEyeY) + up, Y(L.rightEyeY) + up};
  const float open = mouth;
  const int mw = std::max(line * 2, (int)lroundf((L.mouthMinW + (L.mouthMaxW - L.mouthMinW) * (1 - open)) * s));
  const int mh = std::max<int>(m.faceMinLine, lroundf((L.mouthMinH + (L.mouthMaxH - L.mouthMinH) * open) * s));
  const int mx = X(L.mouthX), my = Y(L.mouthY) + up;
  const bool brows = L.browW > 0 && L.browH > 0;
  const int bw = brows ? px(L.browW, r * 2) : 0, bh = brows ? px(L.browH, m.faceMinLine) : 0;
  const int browX[2] = {X(L.leftBrowX), X(L.rightBrowX)}, browY[2] = {Y(L.leftBrowY) + up, Y(L.rightBrowY) + up};

  uint32_t key = 2166136261u;
  for (int v : {(int)style, (int)closed, ex, ey, up, mw, mh, (int)fg, (int)bg, W, H}) mixKey(key, v);
  if (!full && !dirty && key == drawnKey) return;

  // Draw: background, eyes, eyebrows, mouth, then the app's overlay.
  LovyanGFX& g = canvas;
  g.fillScreen(bg);
  for (int i = 0; i < 2; i++) {
    const bool left = i == 0;  // on screen
    const int x = eyeX[i] + ex, y = eyeY[i] + ey;
    Eyes e = style;
    if (e == Eyes::Thinking && !left) e = Eyes::Line;  // one eye narrowed
    bool blinks = e == Eyes::Normal || e == Eyes::Sad || e == Eyes::Angry || e == Eyes::Surprised ||
                  e == Eyes::Thinking || e == Eyes::Sleepy;
    if (closed && blinks) e = Eyes::Line;
    switch (e) {
      case Eyes::Line:
        g.fillRect(x - r, y - line / 2, r * 2 + 1, line, fg);
        break;
      case Eyes::Smile:  // upper arc
        g.fillArc(x, y, r, std::max(0, r - std::max<int>(m.faceMinLine, r / 3)), 180, 360, fg);
        break;
      case Eyes::Cross: {
        float w = std::max(1.0f, line / 2.0f);
        g.drawWideLine(x - r, y - r, x + r, y + r, w, fg);
        g.drawWideLine(x - r, y + r, x + r, y - r, w, fg);
        break;
      }
      case Eyes::Surprised:
        g.fillCircle(x, y, r * 7 / 5, fg);
        break;
      case Eyes::Sleepy:  // lower half
        g.fillArc(x, y, r, 0, 0, 180, fg);
        break;
      case Eyes::Sad:
      case Eyes::Angry: {  // the top cut at a slant: sad lower outside, angry lower inside
        g.fillCircle(x, y, r, fg);
        bool lowOutside = e == Eyes::Sad;
        int x0 = x - r - 1, x1 = x + r + 1, top = y - r - 1;
        int x2 = left == lowOutside ? x0 : x1;
        g.fillTriangle(x0, top, x1, top, x2, y, bg);
        break;
      }
      default:
        g.fillCircle(x, y, r, fg);
    }
    if (brows) {  // slanted like the eyes: sad raised inside, angry lowered inside
      int slant = (style == Eyes::Sad || style == Eyes::Angry) ? std::max(1, bh) : 0;
      int inner = style == Eyes::Sad ? -slant : slant;  // y offset of the inner end
      int xo = left ? browX[i] - bw / 2 : browX[i] + bw / 2, xi = left ? browX[i] + bw / 2 : browX[i] - bw / 2;
      g.drawWideLine(xo, browY[i] - inner, xi, browY[i] + inner, std::max(1.0f, bh / 2.0f), fg);
    }
  }
  g.fillRect(mx - mw / 2, my - mh / 2, mw, mh, fg);
  if (overlay) overlay(g, W, H);

  // Push the area that changed: around the eyes, eyebrows and mouth, now and last time, and the overlay area
  // after invalidate(). The whole rect when the background changed or show() asked for it.
  int reachEye = r * 3 / 2 + line + reach + 2;
  int bx0 = std::min(eyeX[0], eyeX[1]) - reachEye, bx1 = std::max(eyeX[0], eyeX[1]) + reachEye;
  int by0 = std::min(eyeY[0], eyeY[1]) - reachEye - lift, by1 = std::max(eyeY[0], eyeY[1]) + reachEye + lift;
  if (brows) {
    int e = bw / 2 + bh * 2 + 2;
    bx0 = std::min(bx0, std::min(browX[0], browX[1]) - e), bx1 = std::max(bx1, std::max(browX[0], browX[1]) + e);
    by0 = std::min(by0, std::min(browY[0], browY[1]) - e - lift), by1 = std::max(by1, std::max(browY[0], browY[1]) + e);
  }
  bx0 = std::min(bx0, mx - mw / 2 - 2), bx1 = std::max(bx1, mx + (mw + 1) / 2 + 2);
  by0 = std::min(by0, my - mh / 2 - lift - 2), by1 = std::max(by1, my + (mh + 1) / 2 + lift + 2);
  int nb[4] = {bx0, by0, bx1 - bx0, by1 - by0};
  bool whole = full || bg != drawnBg || !box[2] || (dirty && overlay && !ow);
  if (whole) {
    detail::push(canvas, fx, fy);
  } else {
    int ux0 = std::min(nb[0], box[0]), uy0 = std::min(nb[1], box[1]);
    int ux1 = std::max(nb[0] + nb[2], box[0] + box[2]), uy1 = std::max(nb[1] + nb[3], box[1] + box[3]);
    detail::push(canvas, fx, fy, {fx + ux0, fy + uy0, ux1 - ux0, uy1 - uy0});
    if (dirty && overlay) detail::push(canvas, fx, fy, {fx + ox, fy + oy, ow, oh});
  }
  dirty = false;
  for (int i = 0; i < 4; i++) box[i] = nb[i];
  drawnKey = key;
  drawnBg = bg;
  drawnAt = now;
  full = false;
}

}  // namespace m5ui
