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

- [ ] Decide between depending on M5Stack-Avatar (pinning its version) and drawing our own.
- [ ] Requirements: switchable eye shapes (normal, smile, cross), background color fade, drawn from `loop()` without a separate task.

Then:

- [ ] Shrink the face to small eyes in a corner. (InfoView leaves the corner free; the app draws the face there for now.)
- [x] One-line subtitle: `Subtitle`.
- [x] Info panel container, one at a time: `InfoView`, with `PinHeader`, `Terminal`, `Typing` and `StatusList` contents.
- [ ] Lip sync: move the mouth by an audio level the app passes in.

## Out of scope

- App logic, text, speech and conversation, panel content, network services.
- Hardware control (servos, LEDs, touch sensors), including head motion for lip sync. These belong in a separate library.
