// m5-ui showcase: a full-screen face, then every info component beside the same face, small, in the top-left
// corner. Tap the screen for the next one. The face changes its eyes, talks (a made-up voice level) and fades
// its background with a Tween.

#include <M5Unified.h>

#include "m5ui.h"

m5ui::InfoView info;
m5ui::Subtitle subtitle;
m5ui::Badge badge;  // top right: mic on / off, toggled every 3 seconds
m5ui::Face bigFace, smallFace;
m5ui::Tween fade;
uint16_t bgFrom = TFT_BLACK, bgTo = TFT_BLACK;
m5ui::PinHeader header40, header8;
m5ui::Terminal terminal;
m5ui::Typing typing;
m5ui::StatusList status;

enum Screen { SHOW_FACE, SHOW_PINS40, SHOW_PINS8, SHOW_TERMINAL, SHOW_TYPING, SHOW_STATUS, SHOW_COUNT };
int screen = SHOW_FACE;
uint32_t lastStep = 0;
int tick = 0;

m5ui::Face& face() { return screen == SHOW_FACE ? bigFace : smallFace; }

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

void show(int s) {
  screen = s;
  tick = 0;
  // Subtitles start at the left on the terminal screen, centered elsewhere. On the face screen, speech arrives
  // a few characters at a time and the subtitle keeps its newest part in view.
  m5ui::metrics.subtitleAlign = s == SHOW_TERMINAL || s == SHOW_FACE ? m5ui::Align::Left : m5ui::Align::Center;
  m5ui::metrics.subtitleTail = s == SHOW_FACE;
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
  bigFace.begin(0, 0, 320, 240);  // the subtitle band and the badge stay on top
  smallFace.begin(m5ui::metrics.infoX, m5ui::metrics.infoY, m5ui::metrics.cornerW, m5ui::metrics.cornerH);
  smallFace.setLayout(m5ui::FaceLayout::compact());
  badge.set(m5ui::icons::mic);
  if (!info.begin()) Serial.println("m5-ui: out of memory");
  show(SHOW_FACE);
}

void loop() {
  M5.update();
  if (M5.Touch.getDetail().wasClicked()) show((screen + 1) % SHOW_COUNT);
  if (millis() - lastStep >= 100) {
    lastStep = millis();
    step();
  }
  uint32_t now = millis();
  face().setMouth(voiceLevel(now));
  face().setBackground(m5ui::mix565(bgFrom, bgTo, fade.value(now)));
  face().update();
  info.update();
  subtitle.update();
  badge.update();
  delay(5);
}
