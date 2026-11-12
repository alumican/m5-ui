# TODO

In order.

## 1. Install

- [ ] Verify that `lib_deps = https://github.com/alumican/m5-ui.git#v0.1.0` builds in PlatformIO.

## 2. Tokens

- [ ] Let apps override colors, sizes and timings. `m5ui::theme` covers colors only.
- [ ] Move hard-coded values into tokens: button and row heights, margins, popup geometry, font (`Font2`), hold-to-repeat timing (400 ms, 120 ms), drag threshold (8 px).

## 3. Dot font

- [ ] Small bitmap font: digits, symbols, some kana, icons.
- [ ] Precomputed glyphs, fast enough to draw from `loop()`.
- [ ] Apps can add glyphs and icons.

## 4. Face

- [ ] Decide between depending on M5Stack-Avatar (pinning its version) and drawing our own.
- [ ] Requirements: switchable eye shapes (normal, smile, cross), background color fade, drawn from `loop()` without a separate task.

Then:

- [ ] Shrink the face to small eyes in a corner.
- [ ] One-line subtitle.
- [ ] Info panel container, one at a time; the app draws the content.
- [ ] Lip sync: move the mouth by an audio level the app passes in.

## Out of scope

- App logic, text, speech and conversation, panel content, network services.
- Hardware control (servos, LEDs, touch sensors), including head motion for lip sync. These belong in a separate library.
