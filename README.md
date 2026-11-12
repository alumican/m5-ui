# m5-ui

UI components for M5Unified on 320x240 displays (Core2, CoreS3). Tested with M5Unified 0.2.29.

- **Panel**: scrollable settings screen with status lights, buttons, values and a log, closed with the button at the top right.
- **confirm()**: OK / Cancel popup.
- **adjust()**: 0-100 value popup with arrow buttons.
- **choose()**: single-choice list popup.
- **events**: log of the last 12 events, also printed to Serial.

## Install

```ini
lib_deps =
  https://github.com/alumican/m5-ui.git#v0.1.0
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

## events

```cpp
m5ui::events::add("wifi: ok %s", ip);  // stored as "12:34:56 wifi: ok ...", or "+123s ..." before the clock is set
```

## License

MIT
