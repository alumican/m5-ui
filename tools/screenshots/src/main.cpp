// Screenshot firmware for README.md, driven by capture.py over USB serial.
//
// On boot it repeats "ready <name>,<name>,...". Send a name and a newline: it draws that screen, still (no blinks,
// no random looks), reads the display back as a PNG and sends "png <name> <bytes>", the bytes, then "end".
// Then it restarts, so every screen starts fresh. Popups that block run in their own task while the main task
// takes the picture.

#include <M5Unified.h>

#include "m5ui.h"

using namespace m5ui;

static InfoView info;
static Subtitle subtitle;
static Badge badge;
static Face face;
static PinHeader header40, header8;
static Terminal terminal;
static Typing typing;
static StatusList status;
static Circuit circuit;
static Panel panel;

static const char* const SCREENS[] = {"face",     "eyes",   "pins40", "pins8",   "circuit", "terminal", "typing",
                                      "status",   "panel",  "confirm", "adjust", "choose",  "progress"};
static const int SCREEN_COUNT = sizeof SCREENS / sizeof SCREENS[0];

// ---- Content ----

static void setupPins() {
  using K = PinKind;
  struct P {
    const char* name;
    K kind;
  };
  static const P pins[40] = {
      {"3V3", K::Power},   {"5V", K::Power},    {"GPIO2", K::I2c},   {"5V", K::Power},    {"GPIO3", K::I2c},
      {"GND", K::Ground},  {"GPIO4", K::Gpio},  {"GPIO14", K::Uart}, {"GND", K::Ground},  {"GPIO15", K::Uart},
      {"GPIO17", K::Gpio}, {"GPIO18", K::Gpio}, {"GPIO27", K::Gpio}, {"GND", K::Ground},  {"GPIO22", K::Gpio},
      {"GPIO23", K::Gpio}, {"3V3", K::Power},   {"GPIO24", K::Gpio}, {"GPIO10", K::Spi},  {"GND", K::Ground},
      {"GPIO9", K::Spi},   {"GPIO25", K::Gpio}, {"GPIO11", K::Spi},  {"GPIO8", K::Spi},   {"GND", K::Ground},
      {"GPIO7", K::Spi},   {"ID_SD", K::Other}, {"ID_SC", K::Other}, {"GPIO5", K::Gpio},  {"GND", K::Ground},
      {"GPIO6", K::Gpio},  {"GPIO12", K::Gpio}, {"GPIO13", K::Gpio}, {"GND", K::Ground},  {"GPIO19", K::Gpio},
      {"GPIO16", K::Gpio}, {"GPIO26", K::Gpio}, {"GPIO20", K::Gpio}, {"GND", K::Ground},  {"GPIO21", K::Gpio},
  };
  header40.begin(40);
  for (int i = 0; i < 40; i++) header40.setPin(i, String(i + 1), pins[i].name, pins[i].kind);
  header40.highlight(0, 1, "Red wire");
  header40.highlight(5, 2, "Black wire");
  header40.highlight(10, 3, "LED +");

  static const P small[8] = {{"5V", K::Power}, {"GND", K::Ground}, {"SDA", K::I2c}, {"SCL", K::I2c},
                             {"TX", K::Uart},  {"RX", K::Uart},    {"IO1", K::Gpio}, {"IO2", K::Gpio}};
  header8.begin(8);
  for (int i = 0; i < 8; i++) header8.setPin(i, String(i + 1), small[i].name, small[i].kind);
  header8.highlight(2, 1, "Sensor SDA");
  header8.highlight(3, 2, "Sensor SCL");
}

static void setupCircuit() {
  using P = PartKind;
  circuit.setBoard("40-pin board");
  int a = circuit.addLoop("11", "GPIO17", "9", "GND");
  circuit.setResistor(a, circuit.addPart(a, P::Resistor, "330 ohm"), 330);
  circuit.addPart(a, P::Led, "LED");
  int b = circuit.addLoop("13", "GPIO27", "14", "GND");
  circuit.addPart(b, P::Button, "Button");
  int c = circuit.addLoop("15", "GPIO22", "20", "GND");
  circuit.setResistor(c, circuit.addPart(c, P::Resistor, "100 ohm"), 100);
  const uint16_t colors[] = {0xF800, 0xFFE0, 0x07E0};
  const char* names[] = {"Red", "Yellow", "Green"};
  for (int i = 0; i < 3; i++) circuit.setPartColor(c, circuit.addPart(c, P::Led, names[i]), colors[i]);
  circuit.highlight(0);
}

static void setupTerminal() {
  for (int i = 1; i <= 9; i++) {
    if (i == 4) terminal.print("warning: sensor reading is out of range, retrying", Level::Warn);
    else if (i == 7) terminal.print("error: no response", Level::Error);
    else terminal.print("line " + String(i) + ": ok");
  }
}

static void setupTyping() {
  static const char* CODE =
      "#include <M5Unified.h>\n\nvoid setup() {\n  M5.begin();\n  M5.Display.fillScreen(TFT_BLACK);\n}\n\n"
      "void loop() {\n  M5.update();\n  if (M5.Touch.getDetail().wasClicked()) {\n";
  typing.setText(CODE);
  typing.setProgress(strlen(CODE), 220);
}

static void setupStatus() {
  status.set("CPU temp", "47.5C", Light::Ok);
  status.set("Battery", "86%", Light::Ok);
  status.set("Power", "OK", Light::Ok);
  status.set("Wi-Fi", "-61 dBm", Light::Ok);
  status.set("Camera", "Waiting", Light::Wait);
  status.set("Sensor", "Off", Light::Off);
}

static void buildPanel(Panel& p) {
  p.status("Wi-Fi", Light::Ok);
  p.status("Camera", Light::Wait);
  p.status("Sensor", Light::Ng);
  p.status("Speaker", Light::Off);
  p.message("Sensor: no response. Check the cable.");
  p.buttons(3);
  p.button(1, "Retry");
  p.button(2, "Volume");
  p.button(3, "Wi-Fi", false);
  p.heading("Stats");
  p.stat("IP", "192.168.1.23");
  p.stat("Uptime", "3h 12m");
  p.heading("Log");
  for (size_t i = 0; i < events::count(); i++) p.text(events::at(i));
}

// ---- Drawing ----

// Calls update() on everything for ms, so the components settle.
static void settle(uint32_t ms) {
  uint32_t until = millis() + ms;
  while ((int32_t)(until - millis()) > 0) {
    M5.update();
    face.update();
    info.update();
    subtitle.update();
    badge.update();
    delay(5);
  }
}

static void stillFace(Face& f, Eyes eyes, float mouth) {
  f.setAutoBlink(false);
  f.setBreath(false);
  f.setGaze(0, 0);
  f.setEyes(eyes);
  f.setMouth(mouth);
}

// The info screen: a small face in the corner, content, a subtitle.
static void showInfo(const char* title, Content* content, const char* text) {
  face.begin(metrics.infoX, metrics.infoY, metrics.cornerW, metrics.cornerH);
  face.setLayout(FaceLayout::compact());
  stillFace(face, Eyes::Normal, 0.2f);
  face.show();
  info.open(title, content);
  subtitle.set(text);
  settle(800);
}

static void eyesGrid() {
  static const Eyes eyes[] = {Eyes::Normal, Eyes::Smile,     Eyes::Line,     Eyes::Cross, Eyes::Sad,
                              Eyes::Angry,  Eyes::Surprised, Eyes::Thinking, Eyes::Sleepy};
  static const char* names[] = {"Normal", "Smile", "Line", "Cross", "Sad", "Angry", "Surprised", "Thinking", "Sleepy"};
  static Face faces[9];
  const int w = 106, h = 80;
  for (int i = 0; i < 9; i++) {
    faces[i].begin(1 + i % 3 * w, i / 3 * h, w, h);
    faces[i].setLayout(FaceLayout::compact());
    stillFace(faces[i], eyes[i], 0);
    faces[i].show();
  }
  auto& d = M5.Display;
  d.setFont(&fonts::Font0);
  d.setTextColor(theme.dim);
  d.setTextDatum(bottom_center);
  for (int i = 0; i < 9; i++) d.drawString(names[i], 1 + i % 3 * w + w / 2, i / 3 * h + h - 2);
}

static void popupTask(void* arg) {
  const String name = (const char*)arg;
  if (name == "confirm") confirm("Clear Wi-Fi?", "Forget the saved network and stop connecting.", "Clear", "Cancel");
  if (name == "adjust") adjust("Brightness", 70, nullptr);
  if (name == "choose") choose("Size", {"Small", "Medium", "Large", "Extra large"}, 1);
  for (;;) delay(1000);
}

static bool draw(const String& name) {
  if (name == "face") {
    face.begin(0, 0, 320, 240);
    stillFace(face, Eyes::Normal, 0.35f);
    face.show();
    badge.set(icons::mic);
    subtitle.set("Hello! I am a face drawn by m5-ui.");
    settle(800);
  } else if (name == "eyes") {
    eyesGrid();
  } else if (name == "pins40") {
    showInfo("40-pin header", &header40, "Connect the wires in order: 1, 2, 3.");
  } else if (name == "pins8") {
    showInfo("8-pin header", &header8, "Small headers fit whole.");
  } else if (name == "circuit") {
    showInfo("Wiring", &circuit, "From a pin, through the parts, back to GND.");
  } else if (name == "terminal") {
    metrics.subtitleAlign = Align::Left;
    showInfo("Terminal", &terminal, "Last lines, colored by level.");
  } else if (name == "typing") {
    showInfo("Typing", &typing, "Text being typed, with progress.");
  } else if (name == "status") {
    showInfo("Status", &status, "");
  } else if (name == "panel") {
    events::add("wifi: connected");
    events::add("camera: waiting for frames");
    events::add("sensor: no response");
    panel.begin(buildPanel);
    panel.open();
  } else if (name == "confirm" || name == "adjust" || name == "choose") {
    static String arg;
    arg = name;
    xTaskCreatePinnedToCore(popupTask, "popup", 8192, (void*)arg.c_str(), 1, nullptr, 1);
    delay(800);
  } else if (name == "progress") {
    static Progress progress;
    progress.open("Updating", "Cancel");
    progress.update(62, "Writing firmware...");
  } else {
    return false;
  }
  return true;
}

static void send(const String& name) {
  size_t len = 0;
  uint8_t* png = (uint8_t*)M5.Display.createPng(&len, 0, 0, M5.Display.width(), M5.Display.height());
  if (!png) {
    Serial.printf("error %s out of memory\n", name.c_str());
    return;
  }
  Serial.printf("png %s %u\n", name.c_str(), (unsigned)len);
  for (size_t i = 0; i < len; i += 512) {
    Serial.write(png + i, std::min((size_t)512, len - i));
    Serial.flush();
  }
  Serial.print("\nend\n");
  Serial.flush();
  free(png);
}

void setup() {
  M5.begin();
  M5.Display.setRotation(1);
  M5.Display.fillScreen(TFT_BLACK);
  Serial.begin(115200);
  timing.badgeBlinkMs = 0;  // the mic dot stays on
  setupPins();
  setupCircuit();
  setupTerminal();
  setupTyping();
  setupStatus();
  info.begin();
}

void loop() {
  static uint32_t readyAt = 0;
  if (millis() - readyAt >= 500) {
    readyAt = millis();
    Serial.print("ready ");
    for (int i = 0; i < SCREEN_COUNT; i++) Serial.printf("%s%s", i ? "," : "", SCREENS[i]);
    Serial.print("\n");
  }
  if (!Serial.available()) return delay(10);
  String name = Serial.readStringUntil('\n');
  name.trim();
  if (!draw(name)) {
    Serial.printf("error %s unknown screen\n", name.c_str());
    return;
  }
  send(name);
  delay(200);
  ESP.restart();
}
