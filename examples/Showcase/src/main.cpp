// m5-ui showcase: a full-screen face, then every info component beside the same face, small, in the top-left
// corner. Tap the screen for the next one. The face changes its eyes, talks (a made-up voice level) and fades
// its background with a Tween.

#include <M5Unified.h>

#include "m5ui.h"

m5ui::InfoView info;
m5ui::Subtitle subtitle;
m5ui::Badge badge;  // top right: mic on / off, toggled every 3 seconds
m5ui::Face bigFace, smallFace, floatFace;  // full screen, InfoView corner, floating over a View
m5ui::View view;                           // full screen, above the subtitle band
m5ui::Tween fade;
uint16_t bgFrom = TFT_BLACK, bgTo = TFT_BLACK;
m5ui::PinHeader header40, header8;
m5ui::Terminal terminal;
m5ui::Typing typing;
m5ui::StatusList status;
m5ui::Circuit circuit;
m5ui::Breadboard breadboard;

enum Screen { SHOW_FACE, SHOW_PINS40, SHOW_PINS8, SHOW_CIRCUIT, SHOW_BREADBOARD, SHOW_BREADBOARD_FULL, SHOW_TERMINAL, SHOW_TYPING, SHOW_STATUS, SHOW_COUNT };
int screen = SHOW_FACE;
uint32_t lastStep = 0;
int tick = 0;

m5ui::Face& face() {
  return screen == SHOW_FACE ? bigFace : screen == SHOW_BREADBOARD_FULL ? floatFace : smallFace;
}

// A made-up voice level: bursts of syllables with pauses between sentences.
float voiceLevel(uint32_t now) {
  if (now % 6000 > 3500) return 0;
  float syllable = fabsf(sinf(now * 0.018f)), wobble = 0.6f + 0.4f * sinf(now * 0.0071f);
  return syllable * wobble;
}

void setupPins() {
  using K = m5ui::PinKind;
  struct P {
    const char* name;
    K kind;
  };
  // A common 40-pin layout. Position i is pin i + 1.
  static const P pins[40] = {
      {"3V3", K::Power},    {"5V", K::Power},     {"GPIO2", K::I2c},    {"5V", K::Power},
      {"GPIO3", K::I2c},    {"GND", K::Ground},   {"GPIO4", K::Gpio},   {"GPIO14", K::Uart},
      {"GND", K::Ground},   {"GPIO15", K::Uart},  {"GPIO17", K::Gpio},  {"GPIO18", K::Gpio},
      {"GPIO27", K::Gpio},  {"GND", K::Ground},   {"GPIO22", K::Gpio},  {"GPIO23", K::Gpio},
      {"3V3", K::Power},    {"GPIO24", K::Gpio},  {"GPIO10", K::Spi},   {"GND", K::Ground},
      {"GPIO9", K::Spi},    {"GPIO25", K::Gpio},  {"GPIO11", K::Spi},   {"GPIO8", K::Spi},
      {"GND", K::Ground},   {"GPIO7", K::Spi},    {"ID_SD", K::Other},  {"ID_SC", K::Other},
      {"GPIO5", K::Gpio},   {"GND", K::Ground},   {"GPIO6", K::Gpio},   {"GPIO12", K::Gpio},
      {"GPIO13", K::Gpio},  {"GND", K::Ground},   {"GPIO19", K::Gpio},  {"GPIO16", K::Gpio},
      {"GPIO26", K::Gpio},  {"GPIO20", K::Gpio},  {"GND", K::Ground},   {"GPIO21", K::Gpio},
  };
  header40.begin(40);
  for (int i = 0; i < 40; i++) header40.setPin(i, String(i + 1), pins[i].name, pins[i].kind);
  header40.highlight(0, 1, "Red wire");     // 3V3
  header40.highlight(5, 2, "Black wire");   // GND
  header40.highlight(10, 3, "LED +");       // GPIO17
  header40.setBlink(true);

  static const P small[8] = {
      {"5V", K::Power}, {"GND", K::Ground}, {"SDA", K::I2c}, {"SCL", K::I2c},
      {"TX", K::Uart},  {"RX", K::Uart},    {"IO1", K::Gpio}, {"IO2", K::Gpio},
  };
  header8.begin(8);
  for (int i = 0; i < 8; i++) header8.setPin(i, String(i + 1), small[i].name, small[i].kind);
  header8.highlight(2, 1, "Sensor SDA");
  header8.highlight(3, 2, "Sensor SCL");
}

// Three loops: an LED with its resistor, a button, three LEDs in a row.
void setupCircuit() {
  using P = m5ui::PartKind;
  circuit.setBoard("40-pin board");
  int a = circuit.addLoop("11", "GPIO17", "9", "GND");
  circuit.setResistor(a, circuit.addPart(a, P::Resistor, "330 ohm"), 330);  // orange, orange, brown, gold
  circuit.addPart(a, P::Led, "LED");
  int b = circuit.addLoop("13", "GPIO27", "14", "GND");
  circuit.addPart(b, P::Button, "Button");
  int c = circuit.addLoop("15", "GPIO22", "20", "GND");
  circuit.setResistor(c, circuit.addPart(c, P::Resistor, "100 ohm"), 100);  // brown, black, brown, gold
  const uint16_t colors[] = {0xF800, 0xFFE0, 0x07E0};
  const char* names[] = {"Red", "Yellow", "Green"};
  for (int i = 0; i < 3; i++) circuit.setPartColor(c, circuit.addPart(c, P::Led, names[i]), colors[i]);
  circuit.setBlink(true);
}

// GPIO17 -> 330 ohm -> LED -> GND on a half-size breadboard, in five steps.
void setupBreadboard() {
  using P = m5ui::PartKind;
  static const char* names[40] = {
      "3V3",    "5V",     "GPIO2",  "5V",     "GPIO3",  "GND",    "GPIO4",  "GPIO14", "GND",    "GPIO15",
      "GPIO17", "GPIO18", "GPIO27", "GND",    "GPIO22", "GPIO23", "3V3",    "GPIO24", "GPIO10", "GND",
      "GPIO9",  "GPIO25", "GPIO11", "GPIO8",  "GND",    "GPIO7",  "ID_SD",  "ID_SC",  "GPIO5",  "GND",
      "GPIO6",  "GPIO12", "GPIO13", "GND",    "GPIO19", "GPIO16", "GPIO26", "GPIO20", "GND",    "GPIO21"};
  breadboard.setBoard("40-pin board", 40);
  for (int i = 0; i < 40; i++) breadboard.setPin(i + 1, names[i]);
  breadboard.addWire("W1", "pin:11", "10a", m5ui::colorByName("yellow"));
  breadboard.setResistor(breadboard.addPart("R1", P::Resistor, "10e", "14e", "330 ohm"), 330);
  breadboard.setPartColor(breadboard.addPart("D1", P::Led, "14c", "15c", "LED"), m5ui::colorByName("red"));
  breadboard.addWire("W2", "15a", "-:15", m5ui::colorByName("black"));
  breadboard.addWire("W3", "pin:9", "-:20", m5ui::colorByName("black"));
  for (const char* id : {"W1", "R1", "D1", "W2", "W3"}) breadboard.addStep(id);
}

void show(int s) {
  screen = s;
  tick = 0;
  // Subtitles start at the left on the terminal screen, centered elsewhere. On the face screen, speech arrives
  // a few characters at a time and the subtitle keeps its newest part in view.
  m5ui::metrics.subtitleAlign = s == SHOW_TERMINAL || s == SHOW_FACE ? m5ui::Align::Left : m5ui::Align::Center;
  m5ui::metrics.subtitleTail = s == SHOW_FACE;
  if (screen != SHOW_BREADBOARD_FULL) {  // not the full-screen breadboard: no floating face, no View
    floatFace.hide();
    view.close();
  }
  if (s == SHOW_BREADBOARD_FULL) {  // the breadboard on the whole screen, the face floating at the top left
    info.close();
    breadboard.setMargins(0, m5ui::metrics.floatY + m5ui::metrics.floatSize, 0, 0);
    breadboard.showAll();
    view.setContent(&breadboard);
    view.show();
    floatFace.show();
    subtitle.set("Full screen, the face floating. Tap here for the next screen.");
    return;
  }
  breadboard.setMargins(0, 0, 0, 0);
  if (s == SHOW_FACE) {
    info.close();
    bigFace.show();
    subtitle.set("");
    return;
  }
  smallFace.show();
  switch (s) {
    case SHOW_PINS40:
      info.open("40-pin header", &header40);
      subtitle.set("Steps 1 to 3 blink. The header turns every 3 seconds to match the board as seen.");
      break;
    case SHOW_PINS8:
      info.open("8-pin header", &header8);
      subtitle.set("Small headers fit whole.");
      break;
    case SHOW_CIRCUIT:
      info.open("Wiring", &circuit);
      subtitle.set("Each loop goes from a pin, through the parts, back to GND.");
      break;
    case SHOW_BREADBOARD:
      breadboard.setStep(0);
      info.open("Breadboard", &breadboard);
      subtitle.set("Drag to turn. Pinch or the zoom buttons to come closer. Tap the title for the next screen.");
      break;
    case SHOW_TERMINAL:
      terminal.clear();
      info.open("Terminal", &terminal);
      subtitle.set("Last lines, colored by level.");
      break;
    case SHOW_TYPING:
      typing.clear();
      info.open("Typing", &typing);
      subtitle.set("Text being typed, with progress.");
      break;
    case SHOW_STATUS:
      info.open("Status", &status);
      subtitle.set("");
      break;
  }
}

static const char* CODE =
    "#include <M5Unified.h>\n\nvoid setup() {\n  M5.begin();\n  M5.Display.fillScreen(TFT_BLACK);\n}\n\n"
    "void loop() {\n  M5.update();\n  if (M5.Touch.getDetail().wasClicked()) {\n    M5.Speaker.tone(2000, 50);\n  }\n}\n";

void step() {
  tick++;
  // Every 2.5 s the next eyes; every 6 s the background fades to the next color.
  static const m5ui::Eyes eyes[] = {m5ui::Eyes::Normal, m5ui::Eyes::Smile,     m5ui::Eyes::Line,
                                    m5ui::Eyes::Cross,  m5ui::Eyes::Sad,       m5ui::Eyes::Angry,
                                    m5ui::Eyes::Surprised, m5ui::Eyes::Thinking, m5ui::Eyes::Sleepy};
  static const uint16_t backgrounds[] = {TFT_BLACK, 0x0010, TFT_BLACK, 0x6000};
  if (tick % 25 == 0) {
    bigFace.setEyes(eyes[tick / 25 % 9]);
    smallFace.setEyes(eyes[tick / 25 % 9]);
  }
  if (tick % 60 == 0) {
    bgFrom = bgTo;
    bgTo = backgrounds[tick / 60 % 4];
    fade.start(1000, m5ui::ease::inOut);
  }
  static int micTick = 0;
  if (++micTick % 30 == 0) badge.set(micTick / 30 % 2 ? m5ui::icons::micOff : m5ui::icons::mic);
  switch (screen) {
    case SHOW_FACE: {  // speech arriving: 3 characters every 100 ms, then a pause
      static const char* SPEECH =
          "Hello! This is a face drawn by m5-ui. Tap the screen to see the info views, one at a time, "
          "with the same face in the corner. ";
      int n = tick % 60 * 3, len = strlen(SPEECH);
      subtitle.set(String(SPEECH).substring(0, std::min(n, len)));
      break;
    }
    case SHOW_CIRCUIT:  // steps: each loop, then the LED of the first one
      if (tick % 20 == 1) {
        int k = tick / 20 % 4;
        circuit.clearHighlights();
        if (k < 3) circuit.highlight(k);
        else circuit.highlight(0, 1);
      }
      break;
    case SHOW_BREADBOARD:  // the next step every 5 s
    case SHOW_BREADBOARD_FULL:
      if (tick % 50 == 0) breadboard.setStep((breadboard.step() + 1) % 5);
      break;
    case SHOW_PINS40:
      if (tick % 30 == 0) header40.setRotation(tick / 30 * 90);
      break;
    case SHOW_TERMINAL:
      if (tick % 5 == 0) {
        int k = tick / 5;
        if (k % 7 == 3) terminal.print("warning: sensor reading is out of range, retrying", m5ui::Level::Warn);
        else if (k % 11 == 5) terminal.print("error: no response", m5ui::Level::Error);
        else terminal.print("line " + String(k) + ": ok");
      }
      break;
    case SHOW_TYPING: {
      int total = strlen(CODE), done = std::min(total, tick);
      typing.setText(String(CODE).substring(0, done));
      typing.setProgress(done, total);
      break;
    }
    case SHOW_STATUS:
      status.set("CPU temp", String(45 + (tick / 10) % 8) + "." + String(tick % 10) + "C", m5ui::Light::Ok);
      status.set("Battery", String(100 - (tick / 5) % 100) + "%", (tick / 5) % 100 > 80 ? m5ui::Light::Ng : m5ui::Light::Ok);
      status.set("Power", (tick / 20) % 2 ? "Low" : "OK", (tick / 20) % 2 ? m5ui::Light::Ng : m5ui::Light::Ok);
      status.set("Wi-Fi", "-61 dBm", m5ui::Light::Ok);
      status.set("Camera", "Waiting", m5ui::Light::Wait);
      status.set("Sensor", "Off", m5ui::Light::Off);
      break;
  }
}

void setup() {
  M5.begin();
  M5.Display.setRotation(1);
  M5.Display.fillScreen(TFT_BLACK);
  setupPins();
  setupCircuit();
  setupBreadboard();
  bigFace.begin(0, 0, 320, 240);  // the subtitle band and the badge stay on top
  smallFace.begin(m5ui::metrics.infoX, m5ui::metrics.infoY, m5ui::metrics.cornerW, m5ui::metrics.cornerH);
  smallFace.setLayout(m5ui::FaceLayout::compact());
  floatFace.beginFloating();
  floatFace.hide();
  view.begin(0, 0, 320, m5ui::metrics.subtitleY);
  badge.set(m5ui::icons::mic);
  if (!info.begin()) Serial.println("m5-ui: out of memory");
  show(SHOW_FACE);
}

void loop() {
  M5.update();
  // The breadboard takes touches in its area; elsewhere a tap goes to the next screen.
  bool used = screen == SHOW_BREADBOARD ? breadboard.touch(info.contentX(), info.contentY())
              : screen == SHOW_BREADBOARD_FULL ? breadboard.touch(view.x(), view.y())
                                               : false;
  if (!used && M5.Touch.getDetail().wasClicked()) show((screen + 1) % SHOW_COUNT);
  if (millis() - lastStep >= 100) {
    lastStep = millis();
    step();
  }
  uint32_t now = millis();
  face().setMouth(voiceLevel(now));
  if (screen != SHOW_BREADBOARD_FULL) face().setBackground(m5ui::mix565(bgFrom, bgTo, fade.value(now)));
  face().update();
  uint32_t t0 = micros();
  info.update();
  view.update();
  if (screen == SHOW_BREADBOARD || screen == SHOW_BREADBOARD_FULL) {  // frames per second and draw time, to Serial
    static uint32_t frames = 0, worst = 0, since = 0;
    uint32_t us = micros() - t0;
    if (us > 1000) frames++, worst = std::max(worst, us);
    if (now - since >= 1000) {
      if (frames) Serial.printf("breadboard: %u frames/s, slowest %u ms\n", (unsigned)frames, (unsigned)(worst / 1000));
      frames = worst = 0;
      since = now;
    }
  }
  subtitle.update();
  badge.update();
  delay(5);
}
