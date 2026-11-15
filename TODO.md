# TODO

In order.

## 1. Install

- [ ] Verify that `lib_deps = https://github.com/alumican/m5-ui.git#v0.1.0` builds in PlatformIO.

## 2. Tokens

- [x] Let apps override colors, sizes and timings: `m5ui::theme`, `m5ui::metrics`, `m5ui::timing`. InfoView, its contents and Subtitle use them throughout.
- [x] Hold-to-repeat timing and drag threshold are tokens.
- [ ] Move the rest of Panel and the popups into tokens: button and row heights, margins, popup geometry, font (`Font2`).

## 3. Dot font

- [ ] Small bitmap font: digits, symbols, some kana, icons.
- [ ] Precomputed glyphs, fast enough to draw from `loop()`.
- [ ] Apps can add glyphs and icons.

## 4. Face

- [x] Own drawing (no M5Stack-Avatar), from `loop()`: `Face`, `FaceLayout`, `Eyes`.
- [x] Eye shapes, blinking, gaze, breathing, lip sync from a level the app passes.
- [x] Small face in the InfoView corner; one-line subtitle; info panel container.
- [x] Background color set by the app every frame; `Tween` for fades.
- [ ] More mouth shapes (smile, o), if wanted.

## Out of scope

- App logic, text, speech and conversation, panel content, network services.
- Hardware control (servos, LEDs, touch sensors), including head motion for lip sync. These belong in a separate library.
- Audio analysis. The app turns the voice into a 0..1 level for `Face::setMouth()`.
