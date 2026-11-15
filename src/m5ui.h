#pragma once
// m5-ui: UI components for M5Unified on 320x240 displays. See README.md.

#include <M5Unified.h>

#include <vector>

namespace m5ui {

enum class Light { Ok, Wait, Ng, Off };

// Tokens. Apps may change them after M5.begin() and before drawing.

// RGB565 colors.
struct Theme {
  uint16_t bg = 0x18E3, line = 0x6B4D, ok = 0x2EA6, wait = 0xFD20, ng = 0xF206, off = 0x6B4D;
  uint16_t button = 0x3A9F, buttonOff = 0x3186, text = TFT_WHITE, sub = 0x6B4D;
  // InfoView, its contents and Subtitle
  uint16_t screen = TFT_BLACK;             // background, to match the face around them
  uint16_t dim = 0x9CD3;                   // secondary text
  uint16_t accent = 0xFFE0;                // highlights, cursor, progress
  uint16_t warn = 0xFD20, error = 0xF206;  // Terminal levels
  uint16_t subtitle = TFT_WHITE, subtitleBg = TFT_BLACK;
  // Badge and icons
  uint16_t badgeBg = 0x18E3;
  uint16_t iconOn = TFT_WHITE, iconOff = 0x6B4D, iconAlert = 0xF206;
  // PinHeader kinds
  uint16_t pinNone = 0x3186, pinPower = 0xF206, pinGround = 0x8410, pinGpio = 0x2EA6;
  uint16_t pinI2c = 0x4D7F, pinSpi = 0xA21E, pinUart = 0xFD20, pinOther = 0xF418;
};
extern Theme theme;

enum class Align : uint8_t { Left, Center };

// Sizes in pixels and fonts. InfoView, its contents and Subtitle use them.
struct Metrics {
  const lgfx::IFont* font = &fonts::Font2;       // text; set a Japanese font (e.g. &fonts::efontJA_16) for kana
  const lgfx::IFont* titleFont = &fonts::Font4;  // InfoView title
  const lgfx::IFont* smallFont = &fonts::Font0;  // numbers on pins
  int16_t pad = 6;
  // InfoView frame. The top-left corner of it (cornerW x cornerH) is left to the app, e.g. for a small face.
  // The title sits right of the corner; the content gets the full width below it.
  int16_t infoX = 0, infoY = 0, infoW = 320, infoH = 216;
  int16_t cornerW = 96, cornerH = 72;
  int16_t titleH = 28;  // title band height when there is no corner
  int16_t subtitleY = 216, subtitleH = 24;
  Align subtitleAlign = Align::Center;  // Left: starts metrics.pad from the left edge
  int16_t lineGap = 2;  // between text lines in Terminal and Typing
  int16_t rowH = 22;    // StatusList row
  int16_t pinPitch = 22, overviewPitch = 7;  // PinHeader: pin spacing, and in the small whole-header map
  int16_t dragPx = 8;   // drag distance that starts scrolling in Panel and choose()
  int16_t badgeX = 288, badgeY = 4, badgeSize = 28;  // Badge square, top right by default
};
extern Metrics metrics;

// Durations in milliseconds.
struct Timing {
  uint16_t blinkMs = 500;          // PinHeader highlight blink, each phase
  uint16_t cursorMs = 500;         // Typing cursor blink, each phase
  uint16_t subtitleSpeed = 60;     // px per second for a subtitle wider than the screen; 0 cuts it with "..."
  uint16_t subtitleHoldMs = 1500;  // pause at both ends of a subtitle scroll
  uint16_t repeatAfterMs = 400, repeatEveryMs = 120;  // adjust() hold-to-repeat
  uint16_t badgeBlinkMs = 600;     // blinking part of an icon (e.g. the dot of icons::mic), each phase; 0 keeps it on
};
extern Timing timing;

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

// Words drawn by components. Set them for another language (with a font that has it, see metrics.font).
struct Words {
  const char* rowFrom = "row %d from top, %s";         // PinHeader legend, upright header: row, side
  const char* columnFrom = "col %d from left, %s";     // PinHeader legend, turned header: column, side
  const char *left = "left", *right = "right", *top = "top", *bottom = "bottom";
};
extern Words words;

// ---- Info screen ----
// InfoView shows one Content at a time in metrics.info*, leaving the top-left corner to the app.
// Everything here is drawn off screen in PSRAM sprites and pushed whole, so it does not flicker.
// Draw from loop(): call update() each time; it redraws only when something changed.

// Content of an InfoView. Subclass it to draw your own, and call invalidate() when it changes.
class Content {
 public:
  virtual ~Content() {}
  // Draws on g, w x h from (0, 0). g is already filled with theme.screen.
  virtual void draw(LovyanGFX& g, int w, int h) = 0;
  // Called by InfoView::update(). Returns true if the look changed with time (blinking).
  virtual bool animate(uint32_t now) { (void)now; return false; }
  void invalidate() { dirty = true; }

 private:
  friend class InfoView;
  bool dirty = true;
};

class InfoView {
 public:
  // Allocates the sprites. open() calls it if needed; call it again after changing metrics. False if out of memory.
  bool begin();
  // Draws the title and the content. content may be nullptr for an empty view.
  void open(const String& title, Content* content);
  void setTitle(const String& title);
  void setContent(Content* content);
  // Draws nothing; redraw your screen afterwards. Keeps the sprites for the next open().
  void close();
  bool isOpen() const { return opened; }
  // Call from loop(). Redraws what changed.
  void update();
  // Draws again, e.g. after something else drew over the view.
  void show();
  // Content size.
  int width() const { return body.width(); }
  int height() const { return body.height(); }

 private:
  M5Canvas head, body;
  String title;
  Content* content = nullptr;
  bool opened = false, headDirty = true, bodyDirty = true;
  int headX = 0, headY = 0, bodyX = 0, bodyY = 0;
  void drawHead();
  void drawBody();
};

enum class PinKind : uint8_t { None, Power, Ground, Gpio, I2c, Spi, Uart, Other };

// 2 x N pin header. Pin colors come from theme.pin*. Highlighted pins get a ring, a step number and, optionally,
// blink. When the header does not fit at metrics.pinPitch, a small map of the whole header is drawn beside a
// close-up of the highlighted pins. A legend at the bottom lists the highlighted pins by step: number, name,
// position as drawn (words.rowFrom / columnFrom) and note.
class PinHeader : public Content {
 public:
  // pins: total, even. Position i is in row i / 2, column i % 2, so position 0 is pin 1 of a common header.
  void begin(int pins);
  int pins() const { return list.size(); }
  // number: printed on the pin (e.g. "11"). name: printed beside it (e.g. "GPIO17").
  void setPin(int index, const String& number, const String& name, PinKind kind);
  // Clockwise, 0 / 90 / 180 / 270, to match the board as seen. 0: position 0 at the top left, rows going down.
  void setRotation(int degrees);
  // step: number shown beside the pin (1, 2, ...), or 0 for none. note: shown in the legend (e.g. "LED +").
  void highlight(int index, int step = 0, const String& note = String());
  void clearHighlights();
  void setBlink(bool on);
  // Legend of the highlighted pins, on by default.
  void setLegend(bool on);

  void draw(LovyanGFX& g, int w, int h) override;
  bool animate(uint32_t now) override;

 private:
  struct Pin {
    String number, name, note;
    PinKind kind;
    uint8_t step;
    bool on;
  };
  std::vector<Pin> list;
  int rotation = 0;
  bool blink = false, phase = true, legend = true;
};

enum class Level : uint8_t { Normal, Warn, Error };

// The last lines of a log, colored by level (theme.text, theme.warn, theme.error).
class Terminal : public Content {
 public:
  // keep: lines kept, newest last.
  explicit Terminal(size_t keep = 32) : keep(keep) {}
  // '\n' starts a new line.
  void print(const String& text, Level level = Level::Normal);
  void clear();
  // true (default): wrap long lines. false: cut them with "...".
  void setWrap(bool on);

  void draw(LovyanGFX& g, int w, int h) override;

 private:
  struct Line {
    String text;
    Level level;
  };
  std::vector<Line> lines;
  size_t keep;
  bool wrapLines = true;
};

// Text being typed: its last lines with a blinking cursor, and a "done / total" progress bar.
class Typing : public Content {
 public:
  // The text typed so far. Only the last 2 KB are kept.
  void setText(const String& typed);
  void append(const String& more);
  // total 0 hides the progress bar.
  void setProgress(int done, int total);
  void clear();

  void draw(LovyanGFX& g, int w, int h) override;
  bool animate(uint32_t now) override;

 private:
  String typed;
  int done = 0, total = 0;
  bool phase = true;
  void trim();
};

// Rows of a colored dot, a name and a value. Wraps into two columns when the rows do not fit.
class StatusList : public Content {
 public:
  // Updates the row with this name, or adds it at the end.
  void set(const String& name, const String& value, Light light);
  void remove(const String& name);
  void clear();

  void draw(LovyanGFX& g, int w, int h) override;

 private:
  struct Row {
    String name, value;
    Light light;
  };
  std::vector<Row> rows;
};

// One line of text in a band at the bottom of the screen (metrics.subtitle*), over the face or an InfoView.
// Text wider than the screen scrolls (timing.subtitle*). Call update() from loop().
class Subtitle {
 public:
  // Draws text, aligned by metrics.subtitleAlign. Empty text clears the band to theme.subtitleBg.
  void set(const String& text);
  void clear() { set(""); }
  const String& text() const { return txt; }
  // Call from loop(). Scrolls long text.
  void update();
  // Draws again, e.g. after the face drew over the band.
  void show();

 private:
  M5Canvas canvas;
  String txt;
  int textW = 0, drawnOffset = -1;
  uint32_t startedAt = 0;
  bool ready();
  int offset(uint32_t now) const;
  void draw(int offset);
};

// A small icon in a fixed square on screen (metrics.badge*), over the face or an InfoView.
// While a Badge is visible, InfoView and Subtitle draw around its square. One Badge at a time.

// Draws an icon in a size x size square from (0, 0), over theme.badgeBg. on alternates every timing.badgeBlinkMs
// when blinks is true, and is always true otherwise. Define your own Icon to add icons.
struct Icon {
  void (*draw)(LovyanGFX& g, int size, bool on);
  bool blinks;
};

namespace icons {
extern const Icon mic;     // listening: theme.iconOn microphone with a blinking theme.iconAlert dot
extern const Icon micOff;  // not listening: theme.iconOff microphone, struck through
}  // namespace icons

class Badge {
 public:
  // Shows icon. Draws only if it changed or was hidden.
  void set(const Icon& icon);
  // Clears the square to theme.screen and lets InfoView and Subtitle draw there again.
  void hide();
  bool visible() const { return icon != nullptr; }
  // Draws again if visible, e.g. after the face drew over it. Pushes a small sprite, cheap to call every frame.
  void show();
  // Call from loop(). Redraws a blinking icon when its phase changes.
  void update();

 private:
  M5Canvas canvas;
  const Icon* icon = nullptr;
  bool phase = true;
  bool phaseAt(uint32_t now) const;
  void draw();
};

namespace events {
const size_t KEEP = 12;
// printf-style. Prefixes HH:MM:SS, or +Ns since boot if the clock is not set. Also prints to Serial.
void add(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
size_t count();
const String& at(size_t i);  // 0 is the newest
}  // namespace events

}  // namespace m5ui
