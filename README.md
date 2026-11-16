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
- **Face**: eyes and a mouth drawn in any rect (full screen, or the InfoView corner): expressions, blinking, gaze, breathing, lip sync, colors.
- **Tween**: easing curves, color mixing and a small time-based tween, e.g. to fade the face background.
- **Badge**: one small icon in a fixed square, top right by default, over the face or an InfoView. Icons: `mic`, `micOff`, or your own.
- **Tokens**: colors, sizes, fonts and timings in `m5ui::theme`, `m5ui::metrics` and `m5ui::timing`.

`examples/Showcase` shows them all (`pio run` in that folder).

## Install

```ini
lib_deps =
  https://github.com/alumican/m5-ui.git#v0.6.1
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
  header.highlight(0, 1, "Red wire");  // step 1, with a note for the legend
  header.highlight(5, 2, "Black wire");
  header.highlight(10, 3, "LED +");
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

- **PinHeader**: a 2 x N header. Position `i` is row `i / 2`, column `i % 2`. Each pin has a number printed on it, a name beside it and a `PinKind` color (`theme.pin*`). `highlight(i, step)` rings a pin and shows its step number; `setBlink(true)` blinks them. `setRotation(0 / 90 / 180 / 270)` turns the drawing clockwise; text stays upright. When the header does not fit at `metrics.pinPitch`, a small map of the whole header is drawn beside a close-up of the highlighted rows. A legend at the bottom lists the highlighted pins by step: number and name, position as drawn (e.g. "row 6 from top, left", or "col 15 from left, top" when turned) and the note; `setLegend(false)` hides it. The position words are in `m5ui::words`.
- **Terminal**: `print(text, Level::Normal / Warn / Error)` adds lines (`'\n'` splits); the last ones that fit are shown, colored by level. `setWrap(false)` cuts long lines instead of wrapping.
- **Typing**: `setText()` or `append()` the text typed so far and `setProgress(done, total)`. Shows the last lines with a blinking cursor and a "done / total" bar.
- **StatusList**: `set(name, value, Light)` adds or updates a row with a colored dot. Rows that do not fit go into a second column.
- **Circuit**: wiring for beginners. A board on the left, and loops beside it, each from a board terminal through parts in series back to another terminal. Parts are pictures: `Led` (dome and two legs, the longer + leg marked), `Resistor` (banded body), `Button`, `Buzzer`, or a `Box` with a name. Wires are thick and colored per segment; dark wires get an outline. Highlight a loop or a part to explain a step.
- **Your own**: subclass `m5ui::Content`, draw in `draw(LovyanGFX& g, int w, int h)` and call `invalidate()` when it changes. Override `animate(now)` to return true when it should be redrawn as time passes.

`Subtitle::set(text)` draws one line in the band at `metrics.subtitleY`, centered or from the left (`metrics.subtitleAlign = m5ui::Align::Left`, `metrics.pad` from the edge). Text wider than the screen scrolls to its end and back to its start at `timing.subtitleSpeed` (0 cuts it with "..."); call `update()` from `loop()`. `set("")` clears the band.

For speech that arrives bit by bit, call `set()` with the whole text so far each time: text that continues the current one keeps the scroll where it is. With `metrics.subtitleTail = true`, the newest part stays in view: older text slides out to the left, catching up faster when far behind. Keep the face above the band, or call `show()` after drawing it.

The default fonts are ASCII only. For Japanese, set a font that has it, e.g. `m5ui::metrics.font = &fonts::efontJA_16;`. Text wraps at spaces, or between characters where there are none.

### Circuit

```cpp
m5ui::Circuit circuit;
using P = m5ui::PartKind;

circuit.setBoard("40-pin board");
int led = circuit.addLoop("11", "GPIO17", "9", "GND");  // from terminal, back to terminal (wire into it: black)
int r = circuit.addPart(led, P::Resistor, "330 ohm");   // wire into it: theme.wire (yellow)
circuit.setResistor(led, r, 330);                       // bands: orange, orange, brown, gold
circuit.addPart(led, P::Led, "LED");                    // its + leg faces "from"
int btn = circuit.addLoop("1", "3V3", "13", "GPIO27", m5ui::theme.wire);
circuit.addPart(btn, P::Button, "Button", m5ui::theme.wirePower);  // red wire from 3V3 to the button
circuit.highlight(led);       // the whole loop; highlight(led, 1) for the LED only
circuit.setBlink(true);
info.open("Wiring", &circuit);
```

- `addLoop(fromNumber, fromName, toNumber, toName, back)`: `back` is the color of the wire into "to" (default `theme.wireGround`). Terminal labels show on two lines when there is room.
- `addPart(loop, kind, label, wire)`: in order from "from" to "to"; `wire` is the color of the wire into the part. `setPartColor()` colors an LED, a button cap, a buzzer or a box; `setReversed()` turns an LED or buzzer so its + leg faces "to", or a resistor so its tolerance band is at the "from" end.
- `setResistor(loop, part, ohms, tolerancePercent = 5)` draws the 4-band color code: two digits, multiplier (gold x0.1, silver x0.01) and tolerance (1, 2, 0.5, 0.25, 0.1, 0.05, 5, 10; 20 has no band), e.g. 1k: brown, black, red, gold; 4.7k: yellow, violet, red, gold. Without it, plain bands. Band colors: `theme.bands` (black to white, gold, silver).
- 1 to 3 loops fit in the default content area; parts grow with the room (`metrics.circuitRowMin`, `circuitRowMax`). More loops scroll to the highlighted one, with arrows.
- Labels use `metrics.smallFont` (ASCII); write "330 ohm" rather than the ohm sign unless your font has it.
- Colors: `theme.board`, `boardPad`, `wire`, `wirePower`, `wireGround`, `wireEdge`, `lead`, `led`, `buttonCap`, `buzzer`, `resistor`, `resistorBand`, `bands`. Thickness: `metrics.wireW`. Blink: `timing.blinkMs`.

## Face

```cpp
m5ui::Face face;

void setup() {
  M5.begin();
  face.begin(0, 0, 320, 240);  // or the InfoView corner: metrics.infoX, infoY, cornerW, cornerH
}

void loop() {
  face.setEyes(m5ui::Eyes::Smile);
  face.setMouth(voiceLevel);   // 0..1, every frame while speaking
  face.update();               // at most every timing.faceFrameMs, only when something changed
}
```

- Draws only inside its rect, background included, in a PSRAM sprite; only the part around the eyes and mouth is pushed unless the background changed. A Subtitle that shows text keeps its band; a visible Badge is drawn into the face, so neither flickers.
- Eyes: `Normal`, `Smile`, `Line`, `Cross`, `Sad`, `Angry`, `Surprised`, `Thinking`, `Sleepy`.
- By itself: blinks (`timing.blink*`, `setAutoBlink()`), looks around (`timing.gaze*`; `setGaze(x, y)` takes over, `setAutoGaze(true)` gives it back), breathes (`timing.breathMs`, `setBreath()`).
- Colors: `setColor()`, `setBackground()`; from the next frame. Defaults: `theme.face`, `theme.faceBg`.
- `onDraw(fn, x, y, w, h)`: `fn(g, w, h)` draws over the face in the sprite before the push, e.g. text that must not flicker. Call `invalidate()` when it changes; `(x, y, w, h)` is the part it draws in (w 0: the whole rect).
- `show()` draws the whole rect again, e.g. after a popup.
- Two faces (full and corner) are two `Face` objects, or one `Face` moved with `begin()` again.

### Layout

`FaceLayout` places the parts in a `width` x `height` face, scaled to the rect (aspect kept, centered). `metrics.faceMinEye` and `faceMinLine` keep small faces readable; `FaceLayout::compact()` has bigger parts for the 96 x 72 corner.

```cpp
m5ui::FaceLayout l;          // the defaults, spelled out
l.width = 320, l.height = 240;
l.eyeR = 8;
l.leftEyeX = 90, l.leftEyeY = 93, l.rightEyeX = 230, l.rightEyeY = 96;
l.mouthX = 163, l.mouthY = 148;
l.mouthMinW = 50, l.mouthMaxW = 90, l.mouthMinH = 4, l.mouthMaxH = 60;  // closed: 90 x 4, open: 50 x 60
l.browW = 32, l.browH = 0;   // browH > 0 draws eyebrows
l.leftBrowX = 96, l.leftBrowY = 67, l.rightBrowX = 230, l.rightBrowY = 72;
l.gaze = 3, l.breath = 3;
face.setLayout(l);
```

### Coming from M5Stack-Avatar

| M5Stack-Avatar | m5-ui |
| --- | --- |
| `avatar.init()`, `avatar.start()` (drawing task) | `face.begin(x, y, w, h)`, then `face.update()` from `loop()` |
| `avatar.draw()` | `face.update()` (or `face.show()` to force) |
| `Face(...)` with `BoundingRect` positions | `FaceLayout` (numbers above match the default `Face`) |
| `setExpression(Neutral / Happy / Sleepy / Doubt / Sad / Angry)` | `setEyes(Normal / Smile / Sleepy / Thinking / Sad / Angry)` |
| custom eye drawables (line, cross) | `Eyes::Line`, `Eyes::Cross` |
| `setMouthOpenRatio(r)` | `setMouth(r)` |
| `setEyeOpenRatio()` for blinking | automatic; `setAutoBlink(false)` to stop |
| `setGaze(v, h)` | `setGaze(x, y)`; automatic otherwise |
| `setBreath()` | automatic; `setBreath(false)` to stop |
| `ColorPalette` primary / background | `setColor()`, `setBackground()` |
| background fade done by hand | `Tween` and `mix565()` every frame |
| `setScale()` / `setPosition()` for a small face | a second `Face` in the corner rect, or `begin()` again |
| drawing over the face (e.g. a corner text) | `onDraw()` + `invalidate()` |
| speech balloon | `Subtitle` |

## Tween

```cpp
m5ui::Tween fade;
uint16_t from = TFT_BLACK, to = TFT_BLACK;

void setMood(uint16_t color) {
  from = m5ui::mix565(from, to, fade.value());  // from where it is now
  to = color;
  fade.start(1000, m5ui::ease::inOut);
}

void loop() {
  face.setBackground(m5ui::mix565(from, to, fade.value()));
  face.update();
}
```

- `Tween::start(ms, curve)`; `value()` is the eased 0..1 (1 when done or never started); `running()`.
- Curves in `m5ui::ease`: `linear`, `inOut` (smoothstep), `inQuad`, `outQuad`, `inOutQuad`, `inCubic`, `outCubic`, `inOutCubic`; or your own `float (*)(float)`.
- `mix565(a, b, t)`, `mix888(a, b, t)` (0xRRGGBB) and `mix(a, b, t)` for numbers.

## Badge

```cpp
m5ui::Badge badge;

badge.set(listening ? m5ui::icons::mic : m5ui::icons::micOff);  // draws only when it changes

void loop() {
  face.update();   // m5ui::Face keeps the badge on top by itself
  badge.update();  // blinks the red dot of icons::mic
  // if you draw over the badge yourself, call badge.show() afterwards: a small sprite, cheap
}
```

- The square is `metrics.badgeX`, `badgeY`, `badgeSize` (default 28 x 28 at the top right). It clears the InfoView title, the content and the subtitle.
- While visible, InfoView, Face and Subtitle draw it into what they push, so it stays on top without flicker. Popups, Panel and your own drawing cover it; call `show()` after them.
- `hide()`: InfoView and Face under the square redraw it at their next `update()`.
- Colors: `theme.badgeBg`, `iconOn`, `iconOff`, `iconAlert`. Blink: `timing.badgeBlinkMs` (0 keeps the dot on).
- Your own icon: a draw function and whether it blinks.

```cpp
void drawWifi(LovyanGFX& g, int size, bool on) { /* draw in size x size from (0, 0) */ }
const m5ui::Icon wifiIcon = {drawWifi, false};
badge.set(wifiIcon);
```

## Tokens

```cpp
m5ui::theme.accent = TFT_CYAN;       // highlights, cursor, progress
m5ui::metrics.cornerW = 120;         // a larger face corner
m5ui::metrics.font = &fonts::efontJA_16;
m5ui::timing.blinkMs = 300;
m5ui::words.rowFrom = "row %d from top (%s)";  // printf: number, then side (words.left / right / top / bottom)
```

`m5ui::words` holds the few words components draw, for another language. Set them after `M5.begin()` and before drawing. InfoView and Subtitle size their sprites on the first `open()` / `set()`; after changing `metrics` sizes, call `info.begin()` again. Panel and the popups still use fixed sizes and `Font2`; they read `theme`, `metrics.dragPx` and `timing.repeat*`.

## events

```cpp
m5ui::events::add("wifi: ok %s", ip);  // stored as "12:34:56 wifi: ok ...", or "+123s ..." before the clock is set
```

## Tests

`tests`: Unity tests that run on a connected CoreS3. They cover the logic: easing, color mixing, Tween, the resistor color code, text wrapping and cutting, and events. Screens are checked by hand with `examples/Showcase`.

```sh
cd tests
pio test                                        # build, upload and run
pio test --without-uploading --without-testing  # build only
```

## License

MIT
