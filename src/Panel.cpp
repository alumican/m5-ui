#include "m5ui.h"

#include <algorithm>

namespace m5ui {

Theme theme;

static constexpr int W = 320, H = 240;
static constexpr int CLOSE_X = 276, CLOSE_Y = 4, CLOSE_S = 40;  // close button, screen coordinates
static constexpr int STATUS_X2 = 132;                          // second status column
static constexpr int BTN_H = 36, GAP = 8, ROW_GAP = 6;
static constexpr int DRAG_PX = 8;  // drag distance that starts scrolling

void Panel::begin(Build b, int contentHeight) {
  build = b;
  contentH = contentHeight;
  canvas = new M5Canvas(&M5.Display);
  canvas->setPsram(true);
  canvas->createSprite(W, contentH);
}

void Panel::open() {
  opened = true;
  waitRelease = M5.Touch.getDetail().isPressed();
  pressed = NONE;
  scrollY = 0;
  dirty = true;
  update(0);
}

void Panel::close() {
  opened = false;
  pressed = NONE;
}

void Panel::show() {
  dirty = true;
  update(0);
}

void Panel::setBusy(int id) {
  busy = id;
  show();
}

// ---- Layout, called from build. Only hashes when drawing is false. ----

void Panel::mix(const String& s) {
  for (size_t i = 0; i < s.length(); i++) hash = (hash ^ (uint8_t)s[i]) * 16777619u;  // FNV-1a
  hash = (hash ^ 0xFF) * 16777619u;
}
void Panel::mix(int v) { mix(String(v)); }

void Panel::endSection() {
  if (statusCol == 1) {  // unfinished status row
    statusCol = 0;
    y += 18;
  }
  if (gridCols) {  // unfinished button grid
    int rows = (gridIndex + gridCols - 1) / gridCols;
    y = gridY + rows * (BTN_H + ROW_GAP);
    gridCols = 0;
  }
}

void Panel::status(const char* label, Light light) {
  if (gridCols) endSection();
  mix(label);
  mix((int)light);
  int x = statusCol == 0 ? 8 : STATUS_X2, cy = y + 9;
  if (drawing) {
    uint16_t c = light == Light::Ok ? theme.ok : light == Light::Wait ? theme.wait : light == Light::Ng ? theme.ng : theme.off;
    canvas->fillCircle(x + 6, cy, 5, c);
    canvas->setTextColor(theme.text);
    canvas->setTextDatum(middle_left);
    canvas->drawString(label, x + 18, cy);
  }
  if (statusCol == 0) {
    statusCol = 1;
  } else {
    statusCol = 0;
    y += 18;
  }
}

int Panel::wrap(const String& src, int x, int top, int w, int lineH, int maxLines, uint16_t color) {
  int yy = top, lines = 0;
  auto put = [&](const String& s) {
    if (maxLines && lines >= maxLines) return;
    if (drawing && yy + lineH <= contentH) {
      canvas->setTextColor(color);
      canvas->setTextDatum(top_left);
      canvas->drawString(s, x, yy);
    }
    yy += lineH;
    lines++;
  };
  String text = src, line;
  while (text.length()) {
    int sp = text.indexOf(' ');
    String word = sp < 0 ? text : text.substring(0, sp);
    text = sp < 0 ? "" : text.substring(sp + 1);
    String joined = line.length() ? line + " " + word : word;
    if (canvas->textWidth(joined) <= w) {
      line = joined;
      continue;
    }
    if (line.length()) put(line);
    while (canvas->textWidth(word) > w) {  // break words wider than the line
      int n = word.length() - 1;
      while (n > 1 && canvas->textWidth(word.substring(0, n)) > w) n--;
      put(word.substring(0, n));
      word = word.substring(n);
    }
    line = word;
  }
  if (line.length()) put(line);
  return yy;
}

void Panel::message(const String& text) {
  endSection();
  mix(text);
  if (drawing) canvas->drawFastHLine(8, y + 4, W - 16, theme.line);
  y = wrap(text, 8, y + 10, W - 16, 16, 2, theme.wait);
}

void Panel::buttons(int columns) {
  endSection();
  gridCols = columns;
  gridIndex = 0;
  gridY = y + 4;
}

void Panel::skip() {
  if (gridCols) gridIndex++;
}

void Panel::button(int id, const char* label, bool enabled, const char* busyLabel) {
  if (!gridCols) buttons(3);
  int cellW = (W - 16 - GAP * (gridCols - 1)) / gridCols;
  int col = gridIndex % gridCols, row = gridIndex / gridCols;
  int x = 8 + col * (cellW + GAP), by = gridY + row * (BTN_H + ROW_GAP);
  gridIndex++;
  mix(id);
  mix(label);
  mix(enabled);
  rects.push_back({id, x, by, cellW, BTN_H, enabled});
  if (!drawing) return;
  uint16_t fill = theme.button, color = theme.text;
  const char* shown = label;
  if (busy == id) {
    fill = theme.wait;
    if (busyLabel) shown = busyLabel;
  } else if (!enabled) {
    fill = theme.buttonOff;
    color = theme.sub;
  } else if (pressed == id) {
    fill = TFT_WHITE;
    color = theme.bg;
  }
  canvas->fillRoundRect(x, by, cellW, BTN_H, 6, fill);
  canvas->setTextColor(color);
  canvas->setTextDatum(middle_center);
  canvas->drawString(shown, x + cellW / 2, by + BTN_H / 2);
}

void Panel::heading(const char* label) {
  endSection();
  mix(label);
  y += 10;
  if (drawing) {
    canvas->drawFastHLine(8, y + 7, W - 16, theme.line);
    int w = canvas->textWidth(label) + 8;
    canvas->fillRect(12, y, w, 15, theme.bg);
    canvas->setTextColor(theme.sub);
    canvas->setTextDatum(middle_left);
    canvas->drawString(label, 16, y + 7);
  }
  y += 20;
}

static constexpr int STAT_X = 84, STAT_RIGHT = 16;

void Panel::stat(const char* label, const String& value) {
  endSection();
  mix(label);
  mix(value);
  if (drawing) {
    canvas->setTextDatum(top_left);
    canvas->setTextColor(theme.sub);
    canvas->drawString(label, 8, y);
  }
  y = std::max(y + 15, wrap(value, STAT_X, y, W - STAT_X - STAT_RIGHT, 15, 0, theme.text));
}

void Panel::text(const String& line) {
  endSection();
  mix(line);
  y = wrap(line, 8, y, W - 16, 15, 0, theme.text);
}

// ---- Draw ----

void Panel::run(bool draw) {
  drawing = draw;
  hash = 2166136261u;
  mix(pressed);
  mix(busy);
  y = 4;
  statusCol = gridCols = gridIndex = 0;
  rects.clear();
  canvas->setFont(&fonts::Font2);
  if (draw) canvas->fillScreen(theme.bg);
  build(*this);
  endSection();
  if (draw) bottom = min(contentH, y + 8);
}

void Panel::update(uint32_t checkMs) {
  if (!opened || !build) return;
  uint32_t now = millis();
  if (!dirty && checkMs && now - checkedAt < checkMs) return;
  checkedAt = now;
  run(false);  // hash only
  if (!dirty && hash == drawnHash) return;
  dirty = false;
  run(true);
  drawnHash = hash;
  push();
}

void Panel::push() {
  scrollY = constrain(scrollY, 0, maxScroll());
  canvas->pushSprite(0, -scrollY);
  auto& d = M5.Display;
  bool down = pressed == CLOSED;
  d.fillRoundRect(CLOSE_X, CLOSE_Y, CLOSE_S, CLOSE_S, 8, down ? TFT_WHITE : TFT_BLACK);
  uint16_t c = down ? TFT_BLACK : TFT_WHITE;
  int m = 12;
  for (int k = -1; k <= 1; k++) {  // bold cross
    d.drawLine(CLOSE_X + m + k, CLOSE_Y + m, CLOSE_X + CLOSE_S - m + k, CLOSE_Y + CLOSE_S - m, c);
    d.drawLine(CLOSE_X + CLOSE_S - m + k, CLOSE_Y + m, CLOSE_X + m + k, CLOSE_Y + CLOSE_S - m, c);
  }
  if (maxScroll() > 0) {
    int top = CLOSE_Y + CLOSE_S + 8, track = H - top - 4;
    int h = max(20, track * H / bottom);
    d.fillRect(316, top, 3, track, theme.bg);
    d.fillRoundRect(316, top + (track - h) * scrollY / maxScroll(), 3, h, 1, theme.line);
  }
}

// ---- Touch ----

int Panel::hit(int x, int sy) const {
  if (x >= CLOSE_X - 4 && sy < CLOSE_Y + CLOSE_S + 4) return CLOSED;
  int cy = sy + scrollY;
  for (const Rect& r : rects)
    if (r.enabled && x >= r.x && x < r.x + r.w && cy >= r.y && cy < r.y + r.h) return r.id;
  return NONE;
}

int Panel::touch(const m5::touch_detail_t& t) {
  if (!opened) return NONE;
  if (waitRelease) {
    if (!t.isPressed()) waitRelease = false;
    return NONE;
  }
  if (t.wasPressed()) {
    pressed = hit(t.x, t.y);
    startY = t.y;
    startScroll = scrollY;
    dragging = false;
    if (pressed == CLOSED) {
      push();
    } else if (pressed != NONE) {
      show();  // highlight
    }
  } else if (t.isPressed() && pressed != CLOSED) {
    int dy = t.y - startY;
    if (!dragging && abs(dy) >= DRAG_PX) {
      dragging = true;
      if (pressed != NONE) {
        pressed = NONE;
        show();  // clear highlight
      }
    }
    if (dragging && startScroll - dy != scrollY) {
      scrollY = startScroll - dy;
      push();
    }
  } else if (t.wasReleased()) {
    int was = pressed;
    pressed = NONE;
    bool dragged = dragging;
    dragging = false;
    if (dragged || was == NONE) return NONE;
    if (hit(t.x, t.y) != was) {  // released elsewhere
      show();
      return NONE;
    }
    if (was == CLOSED) {
      close();
      return CLOSED;
    }
    show();
    return was;
  }
  return NONE;
}

}  // namespace m5ui
