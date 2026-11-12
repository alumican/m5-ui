#include "m5ui.h"

#include <algorithm>

namespace m5ui {

static bool in(int x, int y, int x0, int y0, int w, int h) { return x >= x0 && x < x0 + w && y >= y0 && y < y0 + h; }

static void roundButton(int x, int y, int w, int h, const char* label, bool pressed, uint16_t fill) {
  auto& d = M5.Display;
  d.fillRoundRect(x, y, w, h, 8, pressed ? TFT_WHITE : fill);
  d.setTextColor(pressed ? theme.bg : theme.text);
  d.setTextDatum(middle_center);
  d.drawString(label, x + w / 2, y + h / 2);
}

// ---- confirm ----

namespace {
constexpr int CX = 20, CY = 50, CW = 280, CH = 140;
constexpr int CB_W = 116, CB_H = 36, CB_Y = CY + CH - CB_H - 12;
constexpr int CANCEL_X = CX + 16, OK_X = CX + CW - 16 - CB_W;
}  // namespace

static void drawConfirm(const char* title, const char* message, const char* ok, const char* cancel, int pressed) {
  auto& d = M5.Display;
  d.fillRect(0, 0, 320, 240, TFT_BLACK);  // hide what is behind
  d.fillRoundRect(CX, CY, CW, CH, 10, theme.bg);
  d.drawRoundRect(CX, CY, CW, CH, 10, theme.line);
  d.setFont(&fonts::Font2);
  d.setTextColor(theme.text);
  d.setTextDatum(top_center);
  d.drawString(title, CX + CW / 2, CY + 12);
  // word-wrap, up to 3 lines
  d.setTextColor(theme.sub == theme.line ? 0xBDF7 : theme.sub);
  d.setTextDatum(top_left);
  String text = message, line;
  int y = CY + 36, lines = 0;
  auto put = [&](const String& s) {
    if (lines++ < 3) d.drawString(s, CX + 16, y), y += 16;
  };
  while (text.length()) {
    int sp = text.indexOf(' ');
    String word = sp < 0 ? text : text.substring(0, sp);
    text = sp < 0 ? "" : text.substring(sp + 1);
    String joined = line.length() ? line + " " + word : word;
    if (d.textWidth(joined) <= CW - 32) {
      line = joined;
    } else {
      put(line);
      line = word;
    }
  }
  if (line.length()) put(line);
  roundButton(CANCEL_X, CB_Y, CB_W, CB_H, cancel, pressed == 0, theme.buttonOff);
  roundButton(OK_X, CB_Y, CB_W, CB_H, ok, pressed == 1, theme.button);
}

bool confirm(const char* title, const char* message, const char* ok, const char* cancel) {
  int pressed = -1;
  drawConfirm(title, message, ok, cancel, pressed);
  for (;;) {
    M5.update();
    auto t = M5.Touch.getDetail();
    auto which = [&](int x, int y) {
      if (in(x, y, CANCEL_X, CB_Y, CB_W, CB_H)) return 0;
      if (in(x, y, OK_X, CB_Y, CB_W, CB_H)) return 1;
      if (!in(x, y, CX, CY, CW, CH)) return 2;  // outside: cancel
      return -1;
    };
    if (t.wasPressed()) {
      pressed = which(t.x, t.y);
      drawConfirm(title, message, ok, cancel, pressed);
    } else if (t.wasReleased()) {
      int was = pressed, here = which(t.x, t.y);
      pressed = -1;
      if (was >= 0 && was == here) return was == 1;
      drawConfirm(title, message, ok, cancel, pressed);
    }
    delay(10);
  }
}

// ---- choose ----

namespace {
constexpr int SX = 20, SW = 280, ROW_H = 38, ROW_GAP = 6, S_PAD = 12, S_TITLE = 34;
constexpr int S_MAX_H = 220;  // max popup height
constexpr int LIST_X = SX + S_PAD, LIST_W = SW - S_PAD * 2;
constexpr int DRAG = 8;
}  // namespace

struct ChooseView {
  int n, listH, contentH, y0, h, scroll = 0;
  explicit ChooseView(int n) : n(n) {
    contentH = n * ROW_H + (n - 1) * ROW_GAP;
    listH = std::min(contentH, S_MAX_H - S_TITLE - S_PAD * 2);
    h = S_TITLE + listH + S_PAD * 2;
    y0 = (240 - h) / 2;
  }
  int listY() const { return y0 + S_TITLE + S_PAD; }
  int maxScroll() const { return contentH - listH; }
  void clamp() { scroll = std::max(0, std::min(scroll, maxScroll())); }
  int rowY(int i) const { return listY() + i * (ROW_H + ROW_GAP) - scroll; }
  // option index; -1 between rows, -2 outside the popup
  int at(int x, int y) const {
    if (!in(x, y, SX, y0, SW, h)) return -2;
    if (!in(x, y, LIST_X, listY(), LIST_W, listH)) return -1;
    for (int i = 0; i < n; i++)
      if (y >= rowY(i) && y < rowY(i) + ROW_H) return i;
    return -1;
  }
};

static void drawChooseList(const ChooseView& v, const std::vector<String>& options, int current, int pressed) {
  auto& d = M5.Display;
  d.setClipRect(LIST_X, v.listY(), LIST_W, v.listH);
  d.fillRect(LIST_X, v.listY(), LIST_W, v.listH, theme.bg);
  d.setFont(&fonts::Font2);
  int barW = v.maxScroll() > 0 ? 8 : 0;  // scroll bar
  for (int i = 0; i < v.n; i++) {
    int y = v.rowY(i);
    if (y + ROW_H < v.listY() || y > v.listY() + v.listH) continue;
    bool on = i == current;
    d.fillRoundRect(LIST_X, y, LIST_W - barW, ROW_H, 8, i == pressed ? TFT_WHITE : on ? theme.button : theme.buttonOff);
    uint16_t fg = i == pressed ? theme.bg : theme.text;
    int cx = LIST_X + 18, cy = y + ROW_H / 2;  // radio mark
    d.drawCircle(cx, cy, 6, fg);
    if (on) d.fillCircle(cx, cy, 3, fg);
    d.setTextColor(fg);
    d.setTextDatum(middle_left);
    d.drawString(options[i], LIST_X + 34, cy);
  }
  if (barW) {
    int x = LIST_X + LIST_W - 3, thumb = std::max(16, v.listH * v.listH / v.contentH);
    int ty = v.listY() + (v.listH - thumb) * v.scroll / v.maxScroll();
    d.fillRoundRect(x - 1, v.listY(), 3, v.listH, 1, theme.buttonOff);
    d.fillRoundRect(x - 1, ty, 3, thumb, 1, theme.sub == theme.line ? 0xBDF7 : theme.sub);
  }
  d.clearClipRect();
}

static void drawChoose(const char* title, const ChooseView& v, const std::vector<String>& options, int current,
                       int pressed) {
  auto& d = M5.Display;
  d.fillRect(0, 0, 320, 240, TFT_BLACK);  // hide what is behind
  d.fillRoundRect(SX, v.y0, SW, v.h, 10, theme.bg);
  d.drawRoundRect(SX, v.y0, SW, v.h, 10, theme.line);
  d.setFont(&fonts::Font2);
  d.setTextColor(theme.text);
  d.setTextDatum(top_center);
  d.drawString(title, SX + SW / 2, v.y0 + 12);
  drawChooseList(v, options, current, pressed);
}

int choose(const char* title, const std::vector<String>& options, int current) {
  int n = options.size();
  if (!n) return -1;
  ChooseView v(n);
  if (current >= 0) {  // scroll the current option into view
    v.scroll = current * (ROW_H + ROW_GAP) - (v.listH - ROW_H) / 2;
    v.clamp();
  }
  int pressed = -1, downY = 0, downScroll = 0, downAt = -1;
  bool down = false, dragging = false;
  drawChoose(title, v, options, current, pressed);
  for (;;) {
    M5.update();
    auto t = M5.Touch.getDetail();
    if (t.wasPressed()) {
      down = true;
      dragging = false;
      downY = t.y;
      downScroll = v.scroll;
      downAt = v.at(t.x, t.y);
      pressed = downAt;
      if (pressed >= 0) drawChooseList(v, options, current, pressed);
    } else if (down && t.isPressed()) {
      if (!dragging && v.maxScroll() > 0 && downAt != -2 && abs(t.y - downY) >= DRAG) {
        dragging = true;
        pressed = -1;
      }
      if (dragging) {
        int before = v.scroll;
        v.scroll = downScroll - (t.y - downY);
        v.clamp();
        if (v.scroll != before) drawChooseList(v, options, current, pressed);
      }
    } else if (t.wasReleased()) {
      down = false;
      int here = v.at(t.x, t.y);
      bool tap = !dragging;
      pressed = -1;
      if (tap && downAt == -2 && here == -2) return -1;  // outside: cancel
      if (tap && downAt >= 0 && downAt == here) return here;
      drawChooseList(v, options, current, pressed);
    }
    delay(10);
  }
}

// ---- adjust ----

static constexpr int X = 20, Y = 40, W = 280, H = 160;  // popup
static constexpr int BTN = 48;                          // arrow button size
static constexpr int LEFT_X = X + 12, RIGHT_X = X + W - 12 - BTN, BTN_Y = Y + 46;
static constexpr int BAR_X = LEFT_X + BTN + 12, BAR_W = RIGHT_X - 12 - BAR_X, BAR_Y = BTN_Y + 14, BAR_H = 20;
static constexpr int DONE_W = 120, DONE_H = 34, DONE_X = X + (W - DONE_W) / 2, DONE_Y = Y + H - DONE_H - 12;
static constexpr uint32_t REPEAT_AFTER_MS = 400, REPEAT_EVERY_MS = 120;


enum class Part { None, Left, Right, Done, Inside, Outside };


static Part partAt(int x, int y) {
  if (in(x, y, LEFT_X, BTN_Y, BTN, BTN)) return Part::Left;
  if (in(x, y, RIGHT_X, BTN_Y, BTN, BTN)) return Part::Right;
  if (in(x, y, DONE_X, DONE_Y, DONE_W, DONE_H)) return Part::Done;
  if (in(x, y, X, Y, W, H)) return Part::Inside;
  return Part::Outside;
}

static void drawButton(int x, int y, int w, int h, const char* label, bool pressed) {
  auto& d = M5.Display;
  d.fillRoundRect(x, y, w, h, 8, pressed ? TFT_WHITE : theme.button);
  d.setTextColor(pressed ? theme.bg : TFT_WHITE);
  d.setTextDatum(middle_center);
  d.drawString(label, x + w / 2, y + h / 2);
}

static void drawArrow(int x, bool left, bool pressed) {
  auto& d = M5.Display;
  d.fillRoundRect(x, BTN_Y, BTN, BTN, 8, pressed ? TFT_WHITE : theme.button);
  int cx = x + BTN / 2, cy = BTN_Y + BTN / 2, s = 10;
  uint16_t c = pressed ? theme.bg : TFT_WHITE;
  if (left) d.fillTriangle(cx + s / 2, cy - s, cx + s / 2, cy + s, cx - s, cy, c);
  else d.fillTriangle(cx - s / 2, cy - s, cx - s / 2, cy + s, cx + s, cy, c);
}

static void drawValue(int value) {
  auto& d = M5.Display;
  d.fillRect(BAR_X, BAR_Y - 2, BAR_W, BAR_H + 30, theme.bg);
  d.fillRoundRect(BAR_X, BAR_Y, BAR_W, BAR_H, 4, theme.buttonOff);
  int w = BAR_W * value / 100;
  if (w > 0) d.fillRoundRect(BAR_X, BAR_Y, w, BAR_H, 4, theme.ok);
  d.setFont(&fonts::Font2);
  d.setTextColor(TFT_WHITE);
  d.setTextDatum(top_center);
  d.drawString(String(value), BAR_X + BAR_W / 2, BAR_Y + BAR_H + 6);
}

static void drawAll(const char* title, int value, Part pressed) {
  auto& d = M5.Display;
  d.fillRect(0, 0, 320, 240, TFT_BLACK);  // hide what is behind
  d.fillRoundRect(X, Y, W, H, 10, theme.bg);
  d.drawRoundRect(X, Y, W, H, 10, theme.line);
  d.setFont(&fonts::Font2);
  d.setTextColor(TFT_WHITE);
  d.setTextDatum(top_center);
  d.drawString(title, X + W / 2, Y + 12);
  drawArrow(LEFT_X, true, pressed == Part::Left);
  drawArrow(RIGHT_X, false, pressed == Part::Right);
  drawValue(value);
  drawButton(DONE_X, DONE_Y, DONE_W, DONE_H, "Done", pressed == Part::Done);
}

int adjust(const char* title, int value, void (*apply)(int), int step) {
  value = constrain(value, 0, 100);
  Part pressed = Part::None;
  uint32_t pressedAt = 0, repeatedAt = 0;
  drawAll(title, value, pressed);
  auto change = [&](int delta) {
    int v = constrain(value + delta, 0, 100);
    if (v == value) return;
    value = v;
    drawValue(value);
    if (apply) apply(value);
  };
  for (;;) {
    M5.update();
    auto t = M5.Touch.getDetail();
    uint32_t now = millis();
    if (t.wasPressed()) {
      pressed = partAt(t.x, t.y);
      pressedAt = repeatedAt = now;
      if (pressed == Part::Left) change(-step);
      if (pressed == Part::Right) change(step);
      drawAll(title, value, pressed);
    } else if (t.isPressed() && (pressed == Part::Left || pressed == Part::Right)) {
      if (now - pressedAt >= REPEAT_AFTER_MS && now - repeatedAt >= REPEAT_EVERY_MS) {
        repeatedAt = now;
        change(pressed == Part::Left ? -step : step);
      }
    } else if (t.wasReleased()) {
      Part was = pressed;
      pressed = Part::None;
      Part here = partAt(t.x, t.y);
      if ((was == Part::Done && here == Part::Done) || (was == Part::Outside && here == Part::Outside)) break;
      drawAll(title, value, pressed);
    }
    delay(10);
  }
  return value;
}

}  // namespace m5ui
