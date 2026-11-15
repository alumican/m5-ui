#include <math.h>

#include <algorithm>

#include "m5ui_internal.h"

namespace m5ui {

void Circuit::clear() {
  board = "";
  loops.clear();
  invalidate();
}

void Circuit::setBoard(const String& name) {
  board = name;
  invalidate();
}

int Circuit::addLoop(const String& fn, const String& fname, const String& tn, const String& tname, uint16_t back) {
  Loop l;
  l.fromNumber = fn, l.fromName = fname, l.toNumber = tn, l.toName = tname, l.back = back, l.on = false;
  loops.push_back(l);
  invalidate();
  return loops.size() - 1;
}

int Circuit::addPart(int loop, PartKind kind, const String& label, uint16_t wire) {
  if (loop < 0 || loop >= (int)loops.size()) return -1;
  Part p = {kind, label, wire, -1, false, false, 0, 5};
  loops[loop].parts.push_back(p);
  invalidate();
  return loops[loop].parts.size() - 1;
}

void Circuit::setPartColor(int loop, int part, uint16_t color) {
  if (loop < 0 || loop >= (int)loops.size() || part < 0 || part >= (int)loops[loop].parts.size()) return;
  loops[loop].parts[part].color = color;
  invalidate();
}

void Circuit::setReversed(int loop, int part, bool r) {
  if (loop < 0 || loop >= (int)loops.size() || part < 0 || part >= (int)loops[loop].parts.size()) return;
  loops[loop].parts[part].reversed = r;
  invalidate();
}

void Circuit::setResistor(int loop, int part, float ohms, float tolerance) {
  if (loop < 0 || loop >= (int)loops.size() || part < 0 || part >= (int)loops[loop].parts.size()) return;
  loops[loop].parts[part].ohms = ohms;
  loops[loop].parts[part].tolerance = tolerance;
  invalidate();
}

// 4-band code: indexes into theme.bands for two digits, multiplier and tolerance (-1: no band). False if the
// value cannot be coded (0, negative, or beyond white x 10^9).
static bool colorCode(float ohms, float tolerance, int out[4]) {
  if (!(ohms > 0)) return false;
  int e = (int)floorf(log10f(ohms)) - 1;  // ohms = dd x 10^e
  long dd = lroundf(ohms / powf(10, e));
  if (dd >= 100) dd /= 10, e++;
  if (e < -2 || e > 9) return false;
  out[0] = dd / 10, out[1] = dd % 10, out[2] = e >= 0 ? e : e == -1 ? 10 : 11;
  static const float tol[] = {1, 2, 0.5f, 0.25f, 0.1f, 0.05f, 5, 10};
  static const int band[] = {1, 2, 5, 6, 7, 8, 10, 11};
  out[3] = -1;
  for (int i = 0; i < 8; i++)
    if (fabsf(tolerance - tol[i]) < tol[i] * 0.01f) out[3] = band[i];
  return true;
}

void Circuit::highlight(int loop, int part) {
  if (loop < 0 || loop >= (int)loops.size()) return;
  if (part < 0) loops[loop].on = true;
  else if (part < (int)loops[loop].parts.size()) loops[loop].parts[part].on = true;
  invalidate();
}

void Circuit::clearHighlights() {
  for (Loop& l : loops) {
    l.on = false;
    for (Part& p : l.parts) p.on = false;
  }
  invalidate();
}

void Circuit::setBlink(bool on) {
  blink = on;
  phase = true;
  invalidate();
}

bool Circuit::animate(uint32_t now) {
  bool p = !blink || !timing.blinkMs || (now / timing.blinkMs) % 2 == 0;
  if (p == phase) return false;
  phase = p;
  for (const Loop& l : loops) {
    if (l.on) return true;
    for (const Part& q : l.parts)
      if (q.on) return true;
  }
  return false;
}

// A thick horizontal or vertical wire; dark colors get an outline so they show on a dark background.
static void wireRect(LovyanGFX& g, int x0, int y0, int x1, int y1, uint16_t color) {
  int t = metrics.wireW;
  int x = std::min(x0, x1) - t / 2, y = std::min(y0, y1) - t / 2;
  int w = abs(x1 - x0) + t, h = abs(y1 - y0) + t;
  if (detail::ink(color) == TFT_WHITE) g.fillRect(x - 1, y - 1, w + 2, h + 2, theme.wireEdge);
  g.fillRect(x, y, w, h, color);
}

static void leg(LovyanGFX& g, int x0, int y0, int x1, int y1) {
  g.drawLine(x0, y0, x1, y1, theme.lead);
  g.drawLine(x0 + 1, y0, x1 + 1, y1, theme.lead);
}

// How far a part reaches above the wire, for its label. LEDs stand above it and put the label below.
static int partAbove(PartKind k, int r) {
  switch (k) {
    case PartKind::Resistor: return std::max(3, r * 11 / 20);
    case PartKind::Button: return std::max(5, r * 4 / 5);
    case PartKind::Buzzer: return std::max(6, r);
    case PartKind::Box: return std::max(6, r * 4 / 5);
    default: return 0;
  }
}

// r: size unit, the LED radius.
void Circuit::drawPart(LovyanGFX& g, const Part& p, int cx, int y, int pw, int r, bool lit) {
  const int x0 = cx - pw / 2, x1 = cx + pw / 2;
  const bool plusLeft = !p.reversed;
  g.setFont(metrics.smallFont);
  switch (p.kind) {
    case PartKind::Led: {
      uint16_t c = p.color >= 0 ? p.color : theme.led;
      int hy = y - 5 - r / 2;                // dome center
      int la = cx - r / 2, lc = cx + r / 2;  // legs under the dome
      if (!plusLeft) std::swap(la, lc);
      // Legs: the anode (+) is longer, with a bend; both run down to the wire and out to the sides.
      leg(g, la, hy + r / 2, la, y);
      leg(g, la, y, plusLeft ? x0 : x1, y);
      leg(g, lc, hy + r / 2, lc, y);
      leg(g, lc, y, plusLeft ? x1 : x0, y);
      g.fillRect(plusLeft ? la - 2 : la + 2, y - 3, 2, 2, theme.lead);  // bend on the long leg
      if (lit) g.fillCircle(cx, hy, r + 3, theme.accent);
      g.fillCircle(cx, hy, r, c);
      g.fillRect(cx - r, hy, r * 2 + 1, r / 2 + 1, c);
      g.fillRect(cx - r - 1, hy + r / 2, r * 2 + 3, 2, c);                     // rim
      g.fillCircle(cx - r / 3, hy - r / 3, std::max(1, r / 4), TFT_WHITE);  // shine
      g.setTextColor(theme.text);
      g.setTextDatum(bottom_center);
      g.drawString("+", plusLeft ? la - 5 : la + 6, y - 2);
      break;
    }
    case PartKind::Resistor: {
      int bh = partAbove(p.kind, r) * 2, bw = std::min(pw - 8, std::max(16, r * 5));
      leg(g, x0, y, cx - bw / 2, y);
      leg(g, cx + bw / 2, y, x1, y);
      if (lit) g.fillRoundRect(cx - bw / 2 - 3, y - bh / 2 - 3, bw + 6, bh + 6, bh / 2 + 3, theme.accent);
      g.fillRoundRect(cx - bw / 2, y - bh / 2, bw, bh, bh / 2, theme.resistor);
      int code[4];
      if (!colorCode(p.ohms, p.tolerance, code)) {  // decorative
        for (int k = 0; k < 3; k++)
          g.fillRect(cx - bw / 4 + k * bw / 6 - 1, y - bh / 2, std::max(2, bw / 12), bh, theme.resistorBand);
        break;
      }
      g.drawRoundRect(cx - bw / 2, y - bh / 2, bw, bh, bh / 2, theme.resistor);
      // Digits and multiplier grouped toward one end, tolerance alone near the other, as on real parts.
      // Bands narrow down on a small body so all four stay apart.
      int band = std::max(1, std::min(bw / 9, 4)), gap = std::max(1, band * 2 / 3);
      int inset = std::max(bh / 2, band + 1);
      for (int k = 0; k < 4; k++) {
        if (code[k] < 0) continue;
        int off = k < 3 ? inset + k * (band + gap) : bw - inset - band;
        int x = p.reversed ? cx + bw / 2 - off - band : cx - bw / 2 + off;
        g.fillRect(x, y - bh / 2 + 1, band, bh - 2, theme.bands[code[k]]);  // framed by the body, so black shows
      }
      break;
    }
    case PartKind::Button: {
      uint16_t c = p.color >= 0 ? p.color : theme.buttonCap;
      int s = partAbove(p.kind, r) * 2;
      leg(g, x0, y, cx - s / 2, y);
      leg(g, cx + s / 2, y, x1, y);
      if (lit) g.fillRoundRect(cx - s / 2 - 3, y - s / 2 - 3, s + 6, s + 6, 4, theme.accent);
      g.fillRoundRect(cx - s / 2, y - s / 2, s, s, 2, theme.buttonOff);
      g.drawRoundRect(cx - s / 2, y - s / 2, s, s, 2, theme.wireEdge);
      g.fillCircle(cx, y, std::max(2, s / 3), c);
      break;
    }
    case PartKind::Buzzer: {
      uint16_t c = p.color >= 0 ? p.color : theme.buzzer;
      int rr = partAbove(p.kind, r);
      leg(g, x0, y, cx - rr, y);
      leg(g, cx + rr, y, x1, y);
      if (lit) g.fillCircle(cx, y, rr + 3, theme.accent);
      g.fillCircle(cx, y, rr, c);
      g.drawCircle(cx, y, rr, theme.wireEdge);
      g.fillCircle(cx, y, std::max(1, rr / 5), TFT_BLACK);
      g.setTextColor(theme.text);
      g.setTextDatum(bottom_center);
      g.drawString("+", plusLeft ? cx - rr - 3 : cx + rr + 4, y - 2);
      break;
    }
    default: {  // Box with its label inside
      uint16_t c = p.color >= 0 ? p.color : theme.bg;
      int bh = partAbove(p.kind, r) * 2, bw = pw - 4;
      if (lit) g.fillRoundRect(x0 - 1, y - bh / 2 - 3, bw + 6, bh + 6, 5, theme.accent);
      g.fillRoundRect(x0 + 2, y - bh / 2, bw, bh, 3, c);
      g.drawRoundRect(x0 + 2, y - bh / 2, bw, bh, 3, theme.text);
      g.setTextColor(detail::ink(c));
      g.setTextDatum(middle_center);
      g.drawString(detail::fit(g, p.label, bw - 4), cx + 1, y);
    }
  }
}

void Circuit::draw(LovyanGFX& g, int w, int h) {
  const int pad = metrics.pad, t = metrics.wireW, n = loops.size();
  bool any = false;
  for (const Loop& l : loops) {
    any = any || l.on;
    for (const Part& p : l.parts) any = any || p.on;
  }

  // Board name on top.
  g.setFont(metrics.font);
  int top = 0;
  if (board.length()) {
    g.setTextColor(theme.text);
    g.setTextDatum(top_left);
    g.drawString(detail::fit(g, board, w - pad * 2), pad, 0);
    top = g.fontHeight() + 2;
  }
  if (!n) return;

  // Rows: as tall as fits between the limits; more loops than fit show a window around the highlighted one.
  int rowH = constrain((h - top) / n, (int)metrics.circuitRowMin, (int)metrics.circuitRowMax);
  int fit = std::max(1, (h - top) / rowH), r0 = 0;
  if (fit < n) {
    int focus = -1;
    for (int i = 0; i < n && focus < 0; i++) {
      if (loops[i].on) focus = i;
      for (const Part& p : loops[i].parts)
        if (p.on && focus < 0) focus = i;
    }
    if (focus >= 0) r0 = constrain(focus - fit / 2, 0, n - fit);
  }
  int shown = std::min(fit, n - r0);

  // Board: wide enough for its terminal labels.
  g.setFont(metrics.smallFont);
  int sh = g.fontHeight(), labelW = 0;
  for (int i = r0; i < r0 + shown; i++) {  // one-line width, enough for two lines too
    const Loop& l = loops[i];
    labelW = std::max(labelW, (int)g.textWidth(l.fromNumber + " " + l.fromName));
    labelW = std::max(labelW, (int)g.textWidth(l.toNumber + " " + l.toName));
  }
  const int padS = t + 4, boardW = std::min(w / 3, labelW + padS + 8), bx = pad + boardW;  // bx: terminal x
  const int rowsH = shown * rowH;
  g.fillRoundRect(pad, top, boardW, rowsH, 4, theme.board);

  // Each row, top down: room for the LED domes, the outgoing wire, a line for LED labels, the return wire.
  const int r = constrain((rowH - 27) / 2, 4, 14);
  for (int k = 0; k < shown; k++) {
    const Loop& l = loops[r0 + k];
    const int ry = top + k * rowH, y1 = ry + 8 + r * 3 / 2, y2 = ry + rowH - t / 2 - 2;
    const bool loopLit = l.on && phase;
    if (l.on) g.fillRoundRect(bx + 2, ry + 1, w - bx - pad - 2, rowH - 2, 4, theme.bg);

    // Terminals: pad on the board edge, number and name left of it, on two lines if there is room.
    g.setFont(metrics.smallFont);
    const bool twoLines = y2 - y1 >= sh * 3 + 3;  // "from" around its wire, "to" above its wire
    for (int j = 0; j < 2; j++) {
      int ty = j ? y2 : y1;
      const String& num = j ? l.toNumber : l.fromNumber;
      const String& name = j ? l.toName : l.fromName;
      g.fillRect(bx - padS + 1, ty - padS / 2, padS, padS, theme.boardPad);
      g.setTextColor(loopLit ? theme.accent : theme.text);
      int lx = bx - padS - 2;
      if (twoLines && num.length() && name.length()) {
        g.setTextDatum(bottom_right);
        g.drawString(num, lx, j ? ty - sh - 1 : ty);
        g.setTextDatum(j ? bottom_right : top_right);
        g.drawString(name, lx, j ? ty : ty + 1);
      } else {
        g.setTextDatum(middle_right);
        g.drawString(name.length() ? num + " " + name : num, lx, ty);
      }
    }

    // Parts spread along the outgoing wire; the return runs along the bottom.
    const int xr = w - pad - t, np = l.parts.size();
    const int x0 = bx + 2, span = xr - 6 - x0;
    const int slot = np ? span / np : span;
    // Part width: resistors longer, so their bands stay apart.
    auto widthOf = [&](const Part& p) {
      return std::min(slot - 6, p.kind == PartKind::Resistor ? std::max(r * 6, 32) : std::max(r * 4, 24));
    };
    int xPrev = bx;
    for (int i = 0; i < np; i++) {
      int cx = x0 + slot * i + slot / 2, pw = widthOf(l.parts[i]);
      wireRect(g, xPrev, y1, cx - pw / 2, y1, l.parts[i].wire);
      xPrev = cx + pw / 2;
    }
    wireRect(g, xPrev, y1, xr, y1, l.back);
    wireRect(g, xr, y1, xr, y2, l.back);
    wireRect(g, xr, y2, bx, y2, l.back);
    for (int i = 0; i < np; i++) {
      const Part& p = l.parts[i];
      int cx = x0 + slot * i + slot / 2;
      drawPart(g, p, cx, y1, widthOf(p), r, p.on && phase);
      if (p.kind == PartKind::Box || !p.label.length()) continue;
      g.setFont(metrics.smallFont);
      bool lit = (p.on || l.on) && phase;
      g.setTextColor(lit ? theme.accent : any && !p.on && !l.on ? theme.dim : theme.text);
      if (p.kind == PartKind::Led) {
        g.setTextDatum(top_center);
        g.drawString(detail::fit(g, p.label, slot - 2), cx, y1 + t / 2 + 2);
      } else {
        g.setTextDatum(bottom_center);
        g.drawString(detail::fit(g, p.label, slot - 2), cx, y1 - partAbove(p.kind, r) - 2);
      }
    }
  }

  // More loops above or below.
  if (r0 > 0) g.fillTriangle(w - pad - 8, top + 6, w - pad, top + 6, w - pad - 4, top + 1, theme.text);
  if (r0 + shown < n) {
    int b = top + rowsH;
    g.fillTriangle(w - pad - 8, b - 6, w - pad, b - 6, w - pad - 4, b - 1, theme.text);
  }
  g.setTextDatum(top_left);
}

}  // namespace m5ui
