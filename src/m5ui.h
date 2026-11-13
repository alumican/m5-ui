#pragma once
// m5-ui: UI components for M5Unified on 320x240 displays. See README.md.

#include <M5Unified.h>

#include <vector>

namespace m5ui {

enum class Light { Ok, Wait, Ng, Off };

// RGB565 colors.
struct Theme {
  uint16_t bg = 0x18E3, line = 0x6B4D, ok = 0x2EA6, wait = 0xFD20, ng = 0xF206, off = 0x6B4D;
  uint16_t button = 0x3A9F, buttonOff = 0x3186, text = TFT_WHITE, sub = 0x6B4D;
};
extern Theme theme;

class Panel {
 public:
  using Build = void (*)(Panel& p);
  static const int NONE = -1;
  static const int CLOSED = -2;  // returned by touch() when closed with the close button

  // Call after M5.begin(). Allocates a 320 x contentHeight sprite in PSRAM.
  void begin(Build build, int contentHeight = 600);
  // A touch already down is ignored until released.
  void open();
  void close();
  bool isOpen() const { return opened; }
  // Call from loop(). While open, rebuilds every checkMs and redraws if the content changed.
  void update(uint32_t checkMs = 200);
  // Redraws, e.g. after something else drew over the panel.
  void show();
  // Returns a button id when released over it, CLOSED when closed with the close button, otherwise NONE.
  int touch(const m5::touch_detail_t& t);
  // Shows the button as busy, with busyLabel. NONE clears.
  void setBusy(int id);

  // Content, called in order from build.
  void status(const char* label, Light light);  // two per row
  void message(const String& text);              // up to 2 lines
  void buttons(int columns = 3);                 // starts an equal-width grid
  void button(int id, const char* label, bool enabled = true, const char* busyLabel = nullptr);
  void skip();                                   // leaves a grid cell empty
  void heading(const char* label);
  void stat(const char* label, const String& value);
  void text(const String& line);                 // wrapped

 private:
  struct Rect {
    int id, x, y, w, h;
    bool enabled;
  };
  Build build = nullptr;
  M5Canvas* canvas = nullptr;
  int contentH = 600;
  bool opened = false, drawing = false, dirty = true;
  uint32_t hash = 0, drawnHash = 0, checkedAt = 0;
  // Layout
  int y = 0, statusCol = 0, gridCols = 0, gridIndex = 0, gridY = 0;
  std::vector<Rect> rects;
  // Touch
  int pressed = NONE, busy = NONE;
  bool closing = false, dragging = false;
  bool waitRelease = false;
  int startY = 0, startScroll = 0, scrollY = 0, bottom = 240;

  void run(bool draw);
  void mix(const String& s);
  void mix(int v);
  void endSection();
  void push();
  int maxScroll() const { return bottom > 240 ? bottom - 240 : 0; }
  int wrap(const String& text, int x, int top, int w, int lineH, int maxLines, uint16_t color);
  int hit(int x, int y) const;
};

// Popups cover the whole screen and block until closed. Touching outside cancels.

// Returns true if ok is chosen.
bool confirm(const char* title, const char* message, const char* ok = "OK", const char* cancel = "Cancel");
// Changes value (0-100) by step; holding an arrow repeats. Calls apply on each change. Returns the final value.
int adjust(const char* title, int value, void (*apply)(int value), int step = 5);
// current: selected index, or -1. Returns the chosen index, or -1 if cancelled.
int choose(const char* title, const std::vector<String>& options, int current);

// Progress popup with a title, a 0-100 bar and an optional Cancel button. Unlike the popups above it does not
// block: open() it, call update() as work goes on (it polls touch itself, so it works while loop() is blocked),
// then close() it and redraw your screen. Only Cancel reacts to touch.
class Progress {
 public:
  // cancel: label of the Cancel button, or nullptr for none. Draws the popup at 0%.
  void open(const char* title, const char* cancel = nullptr);
  // percent: 0-100, clamped. message: line under the bar; nullptr keeps the previous one.
  // Calls M5.update() and redraws only what changed. Returns false once Cancel has been tapped.
  bool update(int percent, const char* message = nullptr);
  // True once Cancel has been tapped. The popup stays until close(). Reset by open().
  bool cancelled() const { return cancelledFlag; }
  bool isOpen() const { return opened; }
  // Frees the popup. Draws nothing; redraw your screen afterwards.
  void close();

 private:
  M5Canvas row;
  String title, cancel, message;
  int y0 = 0, h = 0, percent = 0, drawnPercent = -1;
  bool opened = false, cancelledFlag = false, messageDirty = false;
  bool down = false, pressed = false, waitRelease = false;
  int touchX = 0, touchY = 0;

  void drawBar();
  void drawMessage();
  void drawCancel();
  bool onCancel(int x, int y) const;
};

namespace events {
const size_t KEEP = 12;
// printf-style. Prefixes HH:MM:SS, or +Ns since boot if the clock is not set. Also prints to Serial.
void add(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
size_t count();
const String& at(size_t i);  // 0 is the newest
}  // namespace events

}  // namespace m5ui
