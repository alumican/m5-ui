#include <stdio.h>

#include <algorithm>

#include "m5ui_internal.h"

namespace m5ui {

static uint16_t kindColor(PinKind k) {
  switch (k) {
    case PinKind::Power: return theme.pinPower;
    case PinKind::Ground: return theme.pinGround;
    case PinKind::Gpio: return theme.pinGpio;
    case PinKind::I2c: return theme.pinI2c;
    case PinKind::Spi: return theme.pinSpi;
    case PinKind::Uart: return theme.pinUart;
    case PinKind::Other: return theme.pinOther;
    default: return theme.pinNone;
  }
}

// Grid cell of row r (0..len-1 among the rows drawn) and column c, turned clockwise by rot.
static void cell(int rot, int r, int c, int len, int& gx, int& gy) {
  switch (rot) {
    case 90: gx = len - 1 - r, gy = c; break;
    case 180: gx = 1 - c, gy = len - 1 - r; break;
    case 270: gx = r, gy = 1 - c; break;
    default: gx = c, gy = r;
  }
}

void PinHeader::begin(int n) {
  n = std::max(2, n + (n & 1));
  Pin blank = {String(), String(), String(), PinKind::None, 0, false};
  list.assign(n, blank);
  invalidate();
}

void PinHeader::setPin(int i, const String& number, const String& name, PinKind kind) {
  if (i < 0 || i >= (int)list.size()) return;
  list[i].number = number;
  list[i].name = name;
  list[i].kind = kind;
  invalidate();
}

void PinHeader::setRotation(int degrees) {
  int r = ((degrees % 360 + 360 + 45) / 90 % 4) * 90;
  if (r == rotation) return;
  rotation = r;
  invalidate();
}

void PinHeader::highlight(int i, int step, const String& note) {
  if (i < 0 || i >= (int)list.size()) return;
  list[i].on = true;
  list[i].step = constrain(step, 0, 255);
  list[i].note = note;
  invalidate();
}

void PinHeader::clearHighlights() {
  for (Pin& p : list) p.on = false, p.step = 0, p.note = "";
  invalidate();
}

void PinHeader::setBlink(bool on) {
  blink = on;
  phase = true;
  invalidate();
}

void PinHeader::setLegend(bool on) {
  legend = on;
  invalidate();
}

bool PinHeader::animate(uint32_t now) {
  bool p = !blink || !timing.blinkMs || (now / timing.blinkMs) % 2 == 0;
  if (p == phase) return false;
  phase = p;
  for (const Pin& pin : list)
    if (pin.on) return true;
  return false;
}

void PinHeader::draw(LovyanGFX& g, int w, int h) {
  int n = list.size();
  if (!n) return;
  const int pad = metrics.pad, rows = n / 2;
  const bool vertical = rotation == 0 || rotation == 180;
  g.setFont(metrics.font);
  const int fh = g.fontHeight();
  bool any = false;
  for (const Pin& p : list) any = any || p.on;
  auto badgeW = [&](const Pin& p) { return p.step ? g.textWidth(String(p.step)) + 6 + 3 : 0; };

  // Legend at the bottom: the highlighted pins by step, in up to half the height.
  std::vector<int> marked;
  for (int i = 0; i < n; i++)
    if (list[i].on) marked.push_back(i);
  std::sort(marked.begin(), marked.end(), [&](int a, int b) {
    int sa = list[a].step ? list[a].step : 256, sb = list[b].step ? list[b].step : 256;
    return sa != sb ? sa < sb : a < b;
  });
  const int lineH = fh + 2;
  int legendN = legend ? std::min<int>(marked.size(), (h / 2) / lineH) : 0;
  if (legendN) h -= legendN * lineH + 5;  // the diagram gets the rest

  // Spacing along the rows: room for a line of text, and in a horizontal header for each label, which may span
  // two pitches (labels then alternate between a near and a far line). Across the rows: metrics.pinPitch.
  int pitch = std::max<int>(metrics.pinPitch, fh + 2), across = metrics.pinPitch, labelW = 0;
  if (!vertical) {
    for (const Pin& p : list) labelW = std::max(labelW, (int)g.textWidth(p.name) + badgeW(p));
    pitch = std::min(std::max<int>(metrics.pinPitch, (labelW + 7) / 2), (w - pad * 2) / 3);
  }
  bool stagger = !vertical && labelW + 4 > pitch;
  int mapRoom = 0;  // horizontal: height left for the map above the close-up
  if (!vertical) {
    if (stagger && pad + metrics.overviewPitch * 2 + 2 + across * 2 + (3 + fh * 2) * 2 > h) {
      stagger = false;  // no room for two label lines a side: one line, wider pitch
      pitch = std::min(std::max<int>(metrics.pinPitch, labelW + 4), (w - pad * 2) / 3);
    }
    int labelsH = (3 + fh * (stagger ? 2 : 1)) * 2;
    across = constrain((h - labelsH) / 2, 12, across);  // smaller pins before cutting labels
    mapRoom = h - labelsH - across * 2;
  }
  const int cw = vertical ? across : pitch, ch = vertical ? pitch : across;  // cell size

  // Rows shown close up; if not all fit, a small map of the whole header goes beside them.
  int along = vertical ? h - pad * 2 : w - pad * 2;
  int len = std::max(1, std::min(rows, along / pitch)), r0 = 0;
  int op = std::min<int>(metrics.overviewPitch, along / rows);
  if (vertical) op = std::max(2, op);
  else op = std::min(op, (mapRoom - 4) / 2);
  // Without room for the map (a turned header above a long legend), the legend still tells the position.
  bool map = len < rows && op >= 2;
  int dx = 0, dy = 0, dw = w, dh = h, ox = 0, oy = 0;
  if (len < rows) {
    int lo = rows, hi = -1, focus = 0, focusStep = 256;
    for (int i = 0; i < n; i++) {
      if (!list[i].on) continue;
      int r = i / 2, s = list[i].step ? list[i].step : 255;
      lo = std::min(lo, r);
      hi = std::max(hi, r);
      if (s < focusStep) focusStep = s, focus = r;
    }
    if (hi >= 0) r0 = hi - lo + 1 <= len ? lo - (len - (hi - lo + 1)) / 2 : focus - len / 2;
    r0 = constrain(r0, 0, rows - len);
  }
  if (map) {
    if (vertical) {
      ox = pad, oy = (h - rows * op) / 2;
      dx = ox + op * 2 + pad, dw = w - dx;
    } else {
      ox = (w - rows * op) / 2, oy = std::min<int>(pad, mapRoom - op * 2 - 2);
      dy = oy + op * 2 + 2, dh = h - dy;
    }
    // Whole header, with a frame around the rows shown close up.
    g.fillRect(ox, oy, vertical ? op * 2 : rows * op, vertical ? rows * op : op * 2, theme.bg);
    for (int i = 0; i < n; i++) {
      int gx, gy;
      cell(rotation, i / 2, i % 2, rows, gx, gy);
      int rad = list[i].on ? op / 2 : std::max(1, op / 2 - 1);
      g.fillCircle(ox + gx * op + op / 2, oy + gy * op + op / 2, rad, list[i].on ? theme.accent : kindColor(list[i].kind));
    }
    int x0, y0, x1, y1;
    cell(rotation, r0, 0, rows, x0, y0);
    cell(rotation, r0 + len - 1, 1, rows, x1, y1);
    g.drawRect(ox + std::min(x0, x1) * op - 1, oy + std::min(y0, y1) * op - 1, (abs(x1 - x0) + 1) * op + 2,
               (abs(y1 - y0) + 1) * op + 2, theme.text);
  }

  // Close-up.
  int gw = vertical ? 2 : len, gh = vertical ? len : 2;
  int gx0 = dx + (dw - gw * cw) / 2, gy0 = dy + (dh - gh * ch) / 2;
  g.fillRoundRect(gx0 - 2, gy0 - 2, gw * cw + 4, gh * ch + 4, 4, theme.bg);
  int pr = std::max(3, std::min(cw, ch) / 2 - 2);
  for (int r = r0; r < r0 + len; r++) {
    for (int c = 0; c < 2; c++) {
      const int i = r * 2 + c;
      const Pin& p = list[i];
      int gx, gy;
      cell(rotation, r - r0, c, len, gx, gy);
      int cx = gx0 + gx * cw + cw / 2, cy = gy0 + gy * ch + ch / 2;
      uint16_t color = kindColor(p.kind);
      bool lit = p.on && phase;
      if (i == 0) {  // pin 1 is square
        if (lit) g.fillRect(cx - pr - 2, cy - pr - 2, pr * 2 + 5, pr * 2 + 5, theme.accent);
        g.fillRect(cx - pr, cy - pr, pr * 2 + 1, pr * 2 + 1, color);
      } else {
        if (lit) g.fillCircle(cx, cy, pr + 2, theme.accent);
        g.fillCircle(cx, cy, pr, color);
      }
      g.setFont(metrics.smallFont);
      g.setTextDatum(middle_center);
      g.setTextColor(detail::ink(color));
      g.drawString(p.number, cx, cy + 1);
      g.setFont(metrics.font);

      // Label: step badge, then the name.
      int lx, ly, align, avail;  // align: -1 right edge at lx, 0 centered, 1 left edge at lx
      if (vertical) {
        ly = cy;
        if (gx == 0) lx = gx0 - 6, align = -1, avail = lx - dx - (map ? 0 : pad);
        else lx = gx0 + cw * 2 + 6, align = 1, avail = w - pad - lx;
      } else {
        bool far = stagger && gx % 2 == 1;
        int off = 3 + fh / 2 + (far ? fh : 0);
        ly = gy == 0 ? gy0 - off : gy0 + ch * 2 + off;
        lx = cx, align = 0, avail = stagger ? pitch * 2 - 4 : pitch - 2;
      }
      int bw = badgeW(p);
      String name = detail::fit(g, p.name, avail - bw);
      int uw = bw + g.textWidth(name);
      int left = align < 0 ? lx - uw : align == 0 ? lx - uw / 2 : lx;
      if (bw) {
        uint16_t bc = lit ? theme.accent : theme.line;
        g.fillRoundRect(left, ly - fh / 2, bw - 3, fh, 3, bc);
        g.setTextDatum(middle_center);
        g.setTextColor(detail::ink(bc));
        g.drawString(String(p.step), left + (bw - 3) / 2, ly);
      }
      g.setTextDatum(middle_left);
      g.setTextColor(lit ? theme.accent : p.on || !any ? theme.text : theme.dim);
      g.drawString(name, left + bw, ly);
    }
  }

  // Legend lines: badge, "number name", position as drawn, note.
  if (legendN) {
    int y = h + 2, badgeCol = 0, idCol = 0, placeCol = 0;
    char place[64];
    std::vector<String> places;
    for (int k = 0; k < legendN; k++) {
      const Pin& p = list[marked[k]];
      int gx, gy;
      cell(rotation, marked[k] / 2, marked[k] % 2, rows, gx, gy);
      if (vertical) snprintf(place, sizeof place, words.rowFrom, gy + 1, gx == 0 ? words.left : words.right);
      else snprintf(place, sizeof place, words.columnFrom, gx + 1, gy == 0 ? words.top : words.bottom);
      places.push_back(place);
      badgeCol = std::max(badgeCol, badgeW(p));
      idCol = std::max(idCol, (int)g.textWidth(p.number + " " + p.name));
      placeCol = std::max(placeCol, (int)g.textWidth(places.back()));
    }
    idCol = std::min(idCol, w / 3);
    g.drawFastHLine(pad, y, w - pad * 2, theme.line);
    y += 3;
    g.setTextDatum(middle_left);
    for (int k = 0; k < legendN; k++, y += lineH) {
      const Pin& p = list[marked[k]];
      int cy = y + lineH / 2, x = pad;
      if (p.step) {
        int bw = badgeW(p) - 3;
        g.fillRoundRect(x, cy - fh / 2, bw, fh, 3, theme.accent);
        g.setTextDatum(middle_center);
        g.setTextColor(detail::ink(theme.accent));
        g.drawString(String(p.step), x + bw / 2, cy);
        g.setTextDatum(middle_left);
      }
      x += badgeCol;
      g.setTextColor(theme.text);
      g.drawString(detail::fit(g, p.number + " " + p.name, idCol), x, cy);
      x += idCol + 8;
      int room = w - pad - x;
      g.setTextColor(theme.dim);
      g.drawString(detail::fit(g, places[k], room), x, cy);
      x += std::min(placeCol, room) + 8;
      if (p.note.length() && x < w - pad) {
        g.setTextColor(theme.accent);
        g.drawString(detail::fit(g, p.note, w - pad - x), x, cy);
      }
    }
  }
  g.setTextDatum(top_left);
}

}  // namespace m5ui
