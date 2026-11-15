# m5-ui

UI components for M5Unified on 320x240 displays (Core2, CoreS3). Tested with M5Unified 0.2.29.

- **Panel**: scrollable settings screen with status lights, buttons, values and a log, closed with the button at the top right.
- **confirm()**: OK / Cancel popup.
- **adjust()**: 0-100 value popup with arrow buttons.
- **choose()**: single-choice list popup.
- **Progress**: non-blocking progress popup with an optional Cancel button.
- **events**: log of the last 12 events, also printed to Serial.
- **InfoView**: one piece of information at a time, beside a small face the app draws in the top-left corner. Contents: **PinHeader**, **Terminal**, **Typing**, **StatusList**, or your own.
- **Subtitle**: one line at the bottom of the screen, over the face or an InfoView.
- **Tokens**: colors, sizes, fonts and timings in `m5ui::theme`, `m5ui::metrics` and `m5ui::timing`.

`examples/Showcase` shows them all (`pio run` in that folder).

## Install

```ini
lib_deps =
  https://github.com/alumican/m5-ui.git#v0.3.0
```

## Panel

Describe the content in a build function; Panel lays it out. Panel hashes the content and redraws only when it changes, so build may run any number of times.

```cpp
#include "m5ui.h"

m5ui::Panel panel;
enum { BTN_RETRY = 1, BTN_VOLUME };

void build(m5ui::Panel& p) {
  p.status("Wi-Fi", WiFi.isConnected() ? m5ui::Light::Ok : m5ui::Light::Ng);
  p.status("Sensor", sensorOk ? m5ui::Light::Ok : m5ui::Light::Off);
  p.message(problem);  // up to 2 lines
  p.buttons(3);        // 3-column grid
  p.button(BTN_RETRY, "Retry", true, "Checking...");
  p.button(BTN_VOLUME, "Volume");
  p.heading("Stats");
  p.stat("IP", WiFi.localIP().toString());
  p.heading("Log");
  for (size_t i = 0; i < m5ui::events::count(); i++) p.text(m5ui::events::at(i));
}

void setup() {
  M5.begin();
  panel.begin(build, 600);  // max content height; allocates a 320x600 sprite in PSRAM
}

void loop() {
  M5.update();
  if (panel.isOpen()) {
    int id = panel.touch(M5.Touch.getDetail());
    if (id == m5ui::Panel::CLOSED) { /* redraw your screen */ }
    if (id == BTN_RETRY) { panel.setBusy(BTN_RETRY); retry(); panel.setBusy(m5ui::Panel::NONE); }
    if (id == BTN_VOLUME) { volume = m5ui::adjust("Volume", volume, apply); panel.show(); }
  }
  panel.update();
}
```

- `touch()` returns a button's id when the touch is released over it.
- Dragging 8 px or more scrolls instead of pressing. The close button and the scroll bar stay fixed.
- A touch already down at `open()` is ignored until released.
- After drawing anything over the open panel, call `show()`.
- Colors are in `m5ui::theme`.

## Popups

```cpp
if (m5ui::confirm("Clear Wi-Fi?", "Forget the saved network.", "Clear", "Cancel")) { ... }
int v = m5ui::adjust("Brightness", 50, [](int v) { M5.Display.setBrightness(v * 255 / 100); });
int i = m5ui::choose("Size", {"Small", "Medium", "Large"}, 0);
```

Popups cover the whole screen and block until closed; redraw your screen afterwards. Touching outside closes them: `confirm()` returns false, `choose()` returns -1, `adjust()` returns the current value.

`Progress` does not block. Call `update()` while working, even from a callback that blocks `loop()`; it polls touch itself and redraws only the bar and message when they change. Only the Cancel button reacts to touch.

```cpp
m5ui::Progress progress;

progress.open("Update", "Cancel");  // nullptr instead of "Cancel" for no button
bool ok = runUpdate([](int done, int total) {
  return progress.update(done * 100 / total, "Writing");  // false once Cancel is tapped
});
progress.close();  // then redraw your screen
```

After Cancel is tapped, `update()` returns false and `cancelled()` is true; the popup stays until `close()`.

## Info screen

InfoView fills `metrics.info*` (default: the screen above the subtitle band) except the top-left corner (`metrics.cornerW` x `cornerH`, default 96 x 72), which is left to the app, e.g. for its face drawn small. The title goes right of the corner; the content gets the full width below it (320 x 140 by default). Everything is drawn in PSRAM sprites and pushed whole, so it does not flicker. Call `update()` from `loop()`; it redraws only when something changed or blinks.

```cpp
m5ui::InfoView info;
m5ui::Subtitle subtitle;
m5ui::PinHeader header;
m5ui::Terminal terminal(32);  // keeps 32 lines

void setup() {
  M5.begin();
  header.begin(40);
  header.setPin(0, "1", "3V3", m5ui::PinKind::Power);   // position 0 is pin 1
  header.setPin(5, "6", "GND", m5ui::PinKind::Ground);
  header.setPin(10, "11", "GPIO17", m5ui::PinKind::Gpio);
  // ... the other pins
}

void showWiring() {
  M5.Display.fillRect(0, 0, 320, 216, TFT_BLACK);  // the face shrinks into the corner; the app draws it
  header.clearHighlights();
  header.highlight(0, 1);   // step 1
  header.highlight(5, 2);   // step 2
  header.highlight(10, 3);  // step 3
  header.setBlink(true);
  header.setRotation(90);   // as the board is seen
  info.open("Wiring", &header);
  subtitle.set("Connect pin 1 first.");
}

void loop() {
  M5.update();
  if (info.isOpen() && wired) info.setContent(&terminal);  // swap the content
  info.update();
  subtitle.update();
}
```

- `open(title, content)` draws; `setTitle()` and `setContent()` change what is shown; `close()` draws nothing, so redraw your face afterwards.
- After drawing over the view, call `show()`. Same for `Subtitle`.
- Contents may be changed at any time, open or not. Changes show at the next `update()`.

Contents:

- **PinHeader**: a 2 x N header. Position `i` is row `i / 2`, column `i % 2`. Each pin has a number printed on it, a name beside it and a `PinKind` color (`theme.pin*`). `highlight(i, step)` rings a pin and shows its step number; `setBlink(true)` blinks them. `setRotation(0 / 90 / 180 / 270)` turns the drawing clockwise; text stays upright. When the header does not fit at `metrics.pinPitch`, a small map of the whole header is drawn beside a close-up of the highlighted rows.
- **Terminal**: `print(text, Level::Normal / Warn / Error)` adds lines (`'\n'` splits); the last ones that fit are shown, colored by level. `setWrap(false)` cuts long lines instead of wrapping.
- **Typing**: `setText()` or `append()` the text typed so far and `setProgress(done, total)`. Shows the last lines with a blinking cursor and a "done / total" bar.
- **StatusList**: `set(name, value, Light)` adds or updates a row with a colored dot. Rows that do not fit go into a second column.
- **Your own**: subclass `m5ui::Content`, draw in `draw(LovyanGFX& g, int w, int h)` and call `invalidate()` when it changes. Override `animate(now)` to return true when it should be redrawn as time passes.

`Subtitle::set(text)` draws one line in the band at `metrics.subtitleY`. Text wider than the screen scrolls back and forth at `timing.subtitleSpeed` (0 cuts it with "..."); call `update()` from `loop()`. `set("")` clears the band. Keep the face above the band, or call `show()` after drawing it.

The default fonts are ASCII only. For Japanese, set a font that has it, e.g. `m5ui::metrics.font = &fonts::efontJA_16;`. Text wraps at spaces, or between characters where there are none.

## Tokens

```cpp
m5ui::theme.accent = TFT_CYAN;       // highlights, cursor, progress
m5ui::metrics.cornerW = 120;         // a larger face corner
m5ui::metrics.font = &fonts::efontJA_16;
m5ui::timing.blinkMs = 300;
```

Set them after `M5.begin()` and before drawing. InfoView and Subtitle size their sprites on the first `open()` / `set()`; after changing `metrics` sizes, call `info.begin()` again. Panel and the popups still use fixed sizes and `Font2`; they read `theme`, `metrics.dragPx` and `timing.repeat*`.

## events

```cpp
m5ui::events::add("wifi: ok %s", ip);  // stored as "12:34:56 wifi: ok ...", or "+123s ..." before the clock is set
```

## License

MIT
