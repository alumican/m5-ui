#include <math.h>

#include <algorithm>

#include "m5ui_internal.h"

namespace m5ui {

// ---- Places and geometry (hole pitches) ----

namespace detail {

static const float RAIL_IN = 7.5f, RAIL_OUT = 8.5f;  // rails past rows a and j: "-" inside, "+" outside
static const float PIN_TOP = 1.9f;                   // header pin tips

float boardHalfW(int columns) { return columns / 2.0f + 1.2f; }
float headerX(int columns) { return -boardHalfW(columns) - 4; }

bool parsePlace(const String& s, Place& out) {
  out = {0, 0, 0};
  if (s.startsWith("pin:")) {
    int n = s.substring(4).toInt();
    if (n <= 0) return false;
    out = {3, (int16_t)n, 0};
    return true;
  }
  if (s.length() >= 3 && (s[0] == '+' || s[0] == '-')) {
    int colon = s.indexOf(':');
    if (colon < 1 || colon > 2) return false;
    bool nearA = colon == 2;
    if (nearA && s[1] != 'a') return false;
    int c = s.substring(colon + 1).toInt();
    if (c <= 0) return false;
    int sign = s[0] == '+' ? 1 : -1;
    out = {2, (int16_t)c, (int8_t)(sign * (nearA ? 2 : 1))};
    return true;
  }
  int i = 0, c = 0;
  while (i < (int)s.length() && isdigit((unsigned char)s[i])) c = c * 10 + (s[i++] - '0');
  if (i == 0 || i != (int)s.length() - 1 || c <= 0) return false;
  char r = tolower(s[i]);
  if (r < 'a' || r > 'j') return false;
  out = {1, (int16_t)c, (int8_t)(r - 'a')};
  return true;
}

bool placePosition(const Place& p, int columns, int pins, float& x, float& y, float& z) {
  z = 0;
  switch (p.type) {
    case 1:
      if (p.a > columns) return false;
      x = p.a - (columns + 1) / 2.0f;
      y = p.b < 5 ? -1.5f - (4 - p.b) : 1.5f + (p.b - 5);  // a-e below the groove, f-j above
      return true;
    case 2: {
      if (p.a > columns) return false;
      x = p.a - (columns + 1) / 2.0f;
      bool plus = p.b > 0, nearA = abs(p.b) == 2;
      float d = plus ? RAIL_OUT : RAIL_IN;
      y = nearA ? -d : d;
      return true;
    }
    case 3: {
      if (p.a > pins) return false;
      int i = p.a - 1, rows = pins / 2;
      x = headerX(columns) + (i % 2 ? 0.5f : -0.5f);
      y = (rows - 1) / 2.0f - i / 2;  // pin 1 at the far end
      z = PIN_TOP;
      return true;
    }
  }
  return false;
}

void Camera::prepare() {
  const float k = 0.0174532925f;
  sinYaw = sinf(yaw * k), cosYaw = cosf(yaw * k);
  sinPitch = sinf(pitch * k), cosPitch = cosf(pitch * k);
}

void Camera::project(float x, float y, float z, float& sx, float& sy, float& depth) const {
  const float D = 120;  // camera distance in pitches: mild perspective
  float dx = x - tx, dy = y - ty, dz = z - tz;
  float xr = dx * cosYaw - dy * sinYaw, yr = dx * sinYaw + dy * cosYaw;
  float up = yr * sinPitch + dz * cosPitch;
  depth = -yr * cosPitch + dz * sinPitch;
  float k = scale * D / (D - depth);
  sx = cx + xr * k;
  sy = cy - up * k;
}

}  // namespace detail

using detail::Camera;
using detail::Place;

uint16_t colorByName(const String& name, uint16_t fallback) {
  static const char* names[] = {"black", "brown", "red", "orange", "yellow", "green",
                                "blue",  "violet", "gray", "white", "purple", "grey"};
  static const int index[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 7, 8};
  String n = name;
  n.toLowerCase();
  for (int i = 0; i < 12; i++)
    if (n == names[i]) return theme.bands[index[i]];
  return fallback;
}

// ---- View ----

bool View::begin(int x, int y, int w, int h) {
  vx = x, vy = y;
  dirty = true;
  return detail::makeSprite(canvas, w, h);
}

void View::setContent(Content* c) {
  content = c;
  opened = true;
  dirty = true;
}

void View::show() {
  dirty = true;
  update();
}

void View::update() {
  if (!opened || !canvas.getBuffer()) return;
  if (detail::damaged(damageSeen, {vx, vy, canvas.width(), canvas.height()})) dirty = true;
  bool moved = content && content->animate(millis());
  if (!dirty && !moved && !(content && content->dirty)) return;
  dirty = false;
  if (content) content->dirty = false;
  canvas.fillScreen(theme.screen);
  canvas.setFont(metrics.font);
  canvas.setTextColor(theme.text);
  canvas.setTextDatum(top_left);
  if (content) content->draw(canvas, canvas.width(), canvas.height());
  detail::push(canvas, vx, vy);
}

// ---- Scene ----

void Breadboard::clear() {
  board = "";
  pins.clear();
  items.clear();
  steps.clear();
  current = -1;
  invalidate();
}

void Breadboard::setBoard(const String& label, int n) {
  board = label;
  n = std::max(2, n + (n & 1));
  Pin blank = {String(), PinKind::None};
  pins.assign(n, blank);
  invalidate();
}

void Breadboard::setPin(int number, const String& name, PinKind kind) {
  if (number < 1 || number > (int)pins.size()) return;
  pins[number - 1].name = name;
  pins[number - 1].kind = kind;
  invalidate();
}

void Breadboard::setColumns(int c) {
  columns = constrain(c, 5, 63);
  invalidate();
}

static void toEnd(const String& s, uint8_t& type, int16_t& a, int8_t& b) {
  Place p;
  detail::parsePlace(s, p);
  type = p.type, a = p.a, b = p.b;
}

int Breadboard::addPart(const String& id, PartKind kind, const String& leg1, const String& leg2, const String& label) {
  Item it;
  it.id = id, it.label = label, it.kind = (int8_t)kind;
  toEnd(leg1, it.ends[0].type, it.ends[0].a, it.ends[0].b);
  toEnd(leg2, it.ends[1].type, it.ends[1].a, it.ends[1].b);
  it.color = 0, it.partColor = -1, it.ohms = 0, it.tolerance = 5;
  items.push_back(it);
  invalidate();
  return items.size() - 1;
}

int Breadboard::addWire(const String& id, const String& from, const String& to_, uint16_t color) {
  Item it;
  it.id = id, it.kind = -1;
  toEnd(from, it.ends[0].type, it.ends[0].a, it.ends[0].b);
  toEnd(to_, it.ends[1].type, it.ends[1].a, it.ends[1].b);
  it.color = color, it.partColor = -1, it.ohms = 0, it.tolerance = 5;
  items.push_back(it);
  invalidate();
  return items.size() - 1;
}

void Breadboard::setResistor(int i, float ohms, float tol) {
  if (i < 0 || i >= (int)items.size()) return;
  items[i].ohms = ohms, items[i].tolerance = tol;
  invalidate();
}

void Breadboard::setPartColor(int i, uint16_t c) {
  if (i < 0 || i >= (int)items.size()) return;
  items[i].partColor = c;
  invalidate();
}

void Breadboard::addStep(const String& id) {
  steps.push_back(id);
  invalidate();
}

void Breadboard::clearSteps() {
  steps.clear();
  current = -1;
  invalidate();
}

void Breadboard::setStep(int s) {
  current = s < 0 || s >= (int)steps.size() ? -1 : s;
  if (toStep) zoomTo(stepScale());  // follow the step when close up
  invalidate();
}

int Breadboard::stepItem() const {
  if (current < 0) return -1;
  for (size_t i = 0; i < items.size(); i++)
    if (items[i].id == steps[current]) return i;
  return -1;
}

// ---- Camera ----

static const float LABEL_ROOM = 9;  // pitches left of the header kept for pin labels in the whole view

float Breadboard::fitScale() const {
  float left = detail::headerX(columns) - LABEL_ROOM, right = detail::boardHalfW(columns) + 0.5f;
  float w = vw ? vw : 320, h = vh ? vh : 140;
  return std::min((w - 8) / (right - left), (h - 8) / (detail::BOARD_HALF_D * 2 * 0.9f + 3));
}

float Breadboard::stepScale() const {
  float over = fitScale(), span = 4;
  int i = stepItem();
  if (i >= 0) {
    float p[2][3];
    bool ok = true;
    for (int k = 0; k < 2; k++) {
      Place pl = {items[i].ends[k].type, items[i].ends[k].a, items[i].ends[k].b};
      ok = ok && detail::placePosition(pl, columns, pins.size(), p[k][0], p[k][1], p[k][2]);
    }
    if (ok) span = std::max(4.0f, hypotf(p[0][0] - p[1][0], p[0][1] - p[1][1]) + 3);
  }
  float s = std::min((float)metrics.bbFocusPx * 1.0f, std::min(vw ? vw : 320, vh ? vh : 140) * 0.8f / span);
  return std::max(s, over * 1.6f);
}

void Breadboard::stepCenter(float& x, float& y, float& z) const {
  x = y = z = 0;
  int i = stepItem();
  if (i < 0) return;
  int n = 0;
  for (int k = 0; k < 2; k++) {
    Place pl = {items[i].ends[k].type, items[i].ends[k].a, items[i].ends[k].b};
    float px, py, pz;
    if (!detail::placePosition(pl, columns, pins.size(), px, py, pz)) continue;
    x += px, y += py, z += pz, n++;
  }
  if (n) x /= n, y /= n, z /= n;
}

void Breadboard::zoomTo(float s) {
  from = scale > 0 ? scale : fitScale();
  to = s;
  move.start(timing.viewMoveMs, ease::inOutCubic);
  moving = true;
  invalidate();
}

void Breadboard::showAll() {
  toStep = false;
  zoomTo(fitScale());
}

void Breadboard::showStep() {
  toStep = true;
  zoomTo(stepScale());
}

void Breadboard::setAngles(float y, float p) {
  yaw = y;
  pitch = constrain(p, 10.0f, 90.0f);
  invalidate();
}

bool Breadboard::animate(uint32_t now) {
  bool changed = false;
  if (moving) {
    float v = move.value(now);
    scale = mix(from, to, v);
    if (!move.running(now)) moving = false;
    changed = true;
  }
  bool p = !timing.blinkMs || (now / timing.blinkMs) % 2 == 0;
  if (p != phase) {
    phase = p;
    if (stepItem() >= 0) changed = true;
  }
  return changed;
}

// ---- Touch ----

bool Breadboard::touch(int viewX, int viewY) {
  uint32_t now = millis();
  int n = M5.Touch.getCount();
  if (n >= 2) {
    auto a = M5.Touch.getDetail(0), b = M5.Touch.getDetail(1);
    float d = std::max(1.0f, hypotf(a.x - b.x, a.y - b.y));
    if (!pinching) {
      pinching = true, dragging = false, down = false;
      pinchD0 = d, pinchS0 = scale;
    } else {
      moving = false;
      scale = constrain(pinchS0 * d / pinchD0, fitScale() * 0.8f, (float)metrics.bbMaxPx);
      toStep = scale > (fitScale() + stepScale()) / 2;
      invalidate();
    }
    return true;
  }
  if (pinching) {  // until every finger is up
    if (n == 0) pinching = false;
    return true;
  }
  if (n == 1) {
    auto t = M5.Touch.getDetail(0);
    int x = t.x - viewX, y = t.y - viewY;
    if (!down) {
      if (x < 0 || y < 0 || x >= vw || y >= vh) return false;
      down = true, dragging = false;
      downX = lastX = x, downY = lastY = y;
      downAt = now;
      return true;
    }
    if (!dragging && abs(x - downX) + abs(y - downY) >= metrics.dragPx) dragging = true;
    if (dragging && (x != lastX || y != lastY)) {
      yaw -= (x - lastX) * 0.5f;
      pitch = constrain(pitch + (y - lastY) * 0.4f, 10.0f, 90.0f);
      lastX = x, lastY = y;
      invalidate();
    }
    return true;
  }
  if (!down) return false;
  down = false;
  if (dragging || now - downAt > 500) return true;
  // A tap: the button at the bottom right, or the second of a double tap, switches the view.
  int bs = std::max(20, vh / 6);
  bool onButton = downX >= vw - bs - 4 && downY >= vh - bs - 4;
  if (onButton || (tapAt && now - tapAt < timing.doubleTapMs)) {
    tapAt = 0;
    if (toStep) showAll();
    else showStep();
  } else {
    tapAt = now;
  }
  return true;
}

// ---- Drawing ----

static uint16_t shade(uint16_t c, float k) { return mix565(c, TFT_BLACK, k); }

static void quad(LovyanGFX& g, const float* p, uint16_t c) {  // p: 4 points x, y
  g.fillTriangle(p[0], p[1], p[2], p[3], p[4], p[5], c);
  g.fillTriangle(p[0], p[1], p[4], p[5], p[6], p[7], c);
}

// A box: the top and the sides that face the viewer.
static void box(LovyanGFX& g, const Camera& cam, float x0, float x1, float y0, float y1, float z0, float z1,
                uint16_t top) {
  float sx[8], sy[8], d[8];
  for (int i = 0; i < 8; i++)
    cam.project(i & 1 ? x1 : x0, i & 2 ? y1 : y0, i & 4 ? z1 : z0, sx[i], sy[i], d[i]);
  // sides: corners (bottom pair, top pair); visible when nearer than the box middle
  static const int faces[4][4] = {{0, 1, 5, 4}, {2, 3, 7, 6}, {0, 2, 6, 4}, {1, 3, 7, 5}};
  float mid = 0;
  for (float v : d) mid += v / 8;
  for (int f = 0; f < 4; f++) {
    const int* q = faces[f];
    float fd = (d[q[0]] + d[q[1]] + d[q[2]] + d[q[3]]) / 4;
    if (fd <= mid + 0.01f) continue;
    float p[8] = {sx[q[0]], sy[q[0]], sx[q[1]], sy[q[1]], sx[q[2]], sy[q[2]], sx[q[3]], sy[q[3]]};
    quad(g, p, shade(top, f < 2 ? 0.25f : 0.4f));
  }
  float p[8] = {sx[4], sy[4], sx[5], sy[5], sx[7], sy[7], sx[6], sy[6]};
  quad(g, p, top);
}

static void line3(LovyanGFX& g, const Camera& cam, float x0, float y0, float z0, float x1, float y1, float z1,
                  float r, uint16_t c) {
  float a, b, d, e, f, h;
  cam.project(x0, y0, z0, a, b, d);
  cam.project(x1, y1, z1, e, f, h);
  if (r < 0.75f) g.drawLine(a, b, e, f, c);
  else g.drawWideLine(a, b, e, f, r, c);
}

void Breadboard::draw(LovyanGFX& g, int w, int h) {
  vw = w, vh = h;
  if (scale <= 0) scale = fitScale();
  const int cols = columns, npins = pins.size();
  const float hw = detail::boardHalfW(cols), hd = detail::BOARD_HALF_D, hx = detail::headerX(cols);
  const int rows = npins / 2;
  const int si = stepItem();

  // Camera: the target slides from the scene middle to the step as the zoom goes from whole to close.
  Camera cam;
  cam.yaw = yaw, cam.pitch = pitch, cam.scale = scale, cam.cx = w / 2.0f, cam.cy = h / 2.0f;
  float ax = (hx - LABEL_ROOM + hw + 0.5f) / 2, ay = 0, sxT, syT, szT;
  stepCenter(sxT, syT, szT);
  float over = fitScale(), close = stepScale();
  float k = si >= 0 && close > over ? constrain((scale - over) / (close - over), 0.0f, 1.0f) : 0;
  cam.tx = mix(ax, sxT, k), cam.ty = mix(ay, syT, k), cam.tz = mix(0, szT, k);
  cam.prepare();
  const float s = scale;

  // Where each place is, and whether the step uses it.
  auto pos = [&](const End& e, float& x, float& y, float& z) {
    Place p = {e.type, e.a, e.b};
    return detail::placePosition(p, cols, npins, x, y, z);
  };
  auto stepUses = [&](uint8_t type, int a, int b) {
    if (si < 0) return false;
    for (const End& e : items[si].ends)
      if (e.type == type && e.a == a && (type == 3 || e.b == b)) return true;
    return false;
  };
  auto statusOf = [&](int i) {  // 0 normal, 1 done, 2 now, 3 later
    if (current < 0) return 0;
    for (size_t k2 = 0; k2 < steps.size(); k2++)
      if (steps[k2] == items[i].id) return (int)k2 < current ? 1 : (int)k2 == current ? 2 : 3;
    return 1;
  };

  // ---- Bases, the farther first ----
  auto drawHeader = [&]() {
    if (!npins) return;
    float y0 = -rows / 2.0f - 1, y1 = rows / 2.0f + 1;
    box(g, cam, hx - 2, hx + 2, y0, y1, -0.6f, 0, theme.board);
    box(g, cam, hx - 1, hx + 1, -rows / 2.0f, rows / 2.0f, 0, 1, theme.header);
    for (int i = 0; i < npins; i++) {
      float x, y, z;
      Place p = {3, (int16_t)(i + 1), 0};
      detail::placePosition(p, cols, npins, x, y, z);
      line3(g, cam, x, y, 1, x, y, z, std::max(0.5f, s * 0.12f), theme.boardPad);
      float px, py, pd;
      cam.project(x, y, z, px, py, pd);
      int r = std::max(1, (int)(s * 0.18f));
      g.fillRect(px - r, py - r, r * 2 + 1, r * 2 + 1, i == 0 ? theme.text : theme.boardPad);
    }
  };
  auto drawBreadboard = [&]() {
    box(g, cam, -hw, hw, -hd, hd, -2.5f, 0, theme.bbBody);
    // groove, rail lines
    float p[8], d;
    cam.project(-hw + 0.5f, -0.6f, 0, p[0], p[1], d);
    cam.project(hw - 0.5f, -0.6f, 0, p[2], p[3], d);
    cam.project(hw - 0.5f, 0.6f, 0, p[4], p[5], d);
    cam.project(-hw + 0.5f, 0.6f, 0, p[6], p[7], d);
    quad(g, p, theme.bbGroove);
    for (int side = -1; side <= 1; side += 2) {
      line3(g, cam, -hw + 1, side * (detail::RAIL_OUT + 0.7f), 0, hw - 1, side * (detail::RAIL_OUT + 0.7f), 0,
            std::max(0.5f, s * 0.08f), theme.bbPlus);
      line3(g, cam, -hw + 1, side * (detail::RAIL_IN - 0.7f), 0, hw - 1, side * (detail::RAIL_IN - 0.7f), 0,
            std::max(0.5f, s * 0.08f), theme.bbMinus);
    }
    // holes
    int r = std::max(0, (int)(s * 0.17f));
    auto hole = [&](float x, float y) {
      float px, py, pd;
      cam.project(x, y, 0, px, py, pd);
      g.fillRect(px - r, py - r, r * 2 + 1, r * 2 + 1, theme.bbHole);
    };
    for (int c = 1; c <= cols; c++) {
      float x = c - (cols + 1) / 2.0f;
      for (int row = 0; row < 10; row++) hole(x, row < 5 ? -1.5f - (4 - row) : 1.5f + (row - 5));
      if ((c - 2) % 6 == 5 || c == 1) continue;  // rails: groups of five
      for (float y : {detail::RAIL_IN, detail::RAIL_OUT, -detail::RAIL_IN, -detail::RAIL_OUT}) hole(x, y);
    }
    // column numbers and row letters, when they fit
    if (s >= 6) {
      g.setFont(metrics.smallFont);
      g.setTextDatum(middle_center);
      g.setTextColor(shade(theme.bbBody, 0.55f));
      float px, py, pd;
      for (int c = 1; c <= cols; c++) {
        if (c != 1 && c % 5) continue;
        cam.project(c - (cols + 1) / 2.0f, -6.4f, 0, px, py, pd);
        g.drawString(String(c), px, py);
      }
      for (int row = 0; row < 10; row++) {
        cam.project(-hw + 0.6f, row < 5 ? -1.5f - (4 - row) : 1.5f + (row - 5), 0, px, py, pd);
        char letter[2] = {(char)('a' + row), 0};
        g.drawString(letter, px, py);
      }
    }
  };
  {
    float a, b, dh, db;
    cam.project(hx, 0, 0, a, b, dh);
    cam.project(0, 0, 0, a, b, db);
    if (dh < db) drawHeader(), drawBreadboard();
    else drawBreadboard(), drawHeader();
  }

  // ---- Parts and wires, the farther first ----
  std::vector<std::pair<float, int>> order;
  for (size_t i = 0; i < items.size(); i++) {
    float p[2][3];
    if (!pos(items[i].ends[0], p[0][0], p[0][1], p[0][2]) || !pos(items[i].ends[1], p[1][0], p[1][1], p[1][2])) continue;
    float a, b, d;
    cam.project((p[0][0] + p[1][0]) / 2, (p[0][1] + p[1][1]) / 2, 2, a, b, d);
    order.push_back(std::make_pair(d, (int)i));
  }
  std::sort(order.begin(), order.end());
  for (const auto& o : order) {
    const Item& it = items[o.second];
    int st = statusOf(o.second);
    float dimK = st == 1 ? 0.35f : st == 3 ? 0.65f : 0;
    bool lit = st == 2 && phase;
    auto col = [&](uint16_t c) { return dimK > 0 ? mix565(c, theme.screen, dimK) : c; };
    float p[2][3];
    pos(it.ends[0], p[0][0], p[0][1], p[0][2]);
    pos(it.ends[1], p[1][0], p[1][1], p[1][2]);
    float dx = p[1][0] - p[0][0], dy = p[1][1] - p[0][1], len = std::max(0.01f, hypotf(dx, dy));
    float mx = (p[0][0] + p[1][0]) / 2, my = (p[0][1] + p[1][1]) / 2;

    if (it.kind < 0) {  // wire: an arc above the board
      float rise = 1.2f + len * 0.22f, r = std::max(1.0f, s * 0.2f);
      float px[13], py[13], d;
      for (int k2 = 0; k2 <= 12; k2++) {
        float t = k2 / 12.0f, z = mix(p[0][2], p[1][2], t) + rise * 4 * t * (1 - t);
        cam.project(p[0][0] + dx * t, p[0][1] + dy * t, z, px[k2], py[k2], d);
      }
      uint16_t c = col(it.color);
      bool dark = detail::ink(it.color) == TFT_WHITE;
      if (lit || dark)
        for (int k2 = 0; k2 < 12; k2++)
          g.drawWideLine(px[k2], py[k2], px[k2 + 1], py[k2 + 1], r + (lit ? 2 : 1), lit ? theme.accent : col(theme.wireEdge));
      for (int k2 = 0; k2 < 12; k2++) g.drawWideLine(px[k2], py[k2], px[k2 + 1], py[k2 + 1], r, c);
      continue;
    }

    // Parts stand on their legs.
    const PartKind kind = (PartKind)it.kind;
    float cx, cy, cd;
    const float lift = kind == PartKind::Led ? 1.6f : 1.0f;
    cam.project(mx, my, lift, cx, cy, cd);
    if (lit) {  // a ring around the part
      float rr = std::max(6.0f, s * (kind == PartKind::Resistor ? len / 2 + 0.4f : 1.5f));
      g.drawEllipse(cx, cy, rr, std::max(3.0f, rr * cam.sinPitch), theme.accent);
      g.drawEllipse(cx, cy, rr + 1, std::max(4.0f, (rr + 1) * cam.sinPitch), theme.accent);
    }
    uint16_t lead = col(theme.lead);
    float lr = std::max(0.5f, s * 0.07f);
    switch (kind) {
      case PartKind::Resistor: {
        float ux = dx / len, uy = dy / len, z = 1.0f, bodyL = std::min(len - 0.8f, 2.6f);
        float bx0 = mx - ux * bodyL / 2, by0 = my - uy * bodyL / 2, bx1 = mx + ux * bodyL / 2, by1 = my + uy * bodyL / 2;
        line3(g, cam, p[0][0], p[0][1], 0, p[0][0], p[0][1], z, lr, lead);
        line3(g, cam, p[1][0], p[1][1], 0, p[1][0], p[1][1], z, lr, lead);
        line3(g, cam, p[0][0], p[0][1], z, bx0, by0, z, lr, lead);
        line3(g, cam, bx1, by1, z, p[1][0], p[1][1], z, lr, lead);
        float br = std::max(1.5f, s * 0.42f);
        line3(g, cam, bx0, by0, z, bx1, by1, z, br, col(theme.resistor));
        int code[4];
        if (detail::colorCode(it.ohms, it.tolerance, code)) {  // stripes across the body, on screen
          float ax2, ay2, bx2, by2, dd;
          cam.project(bx0, by0, z, ax2, ay2, dd);
          cam.project(bx1, by1, z, bx2, by2, dd);
          float ex = bx2 - ax2, ey = by2 - ay2, el = std::max(0.01f, hypotf(ex, ey));
          float nx = -ey / el * br * 0.95f, ny = ex / el * br * 0.95f, half = std::max(0.6f, el * 0.045f);
          static const float at[4] = {0.18f, 0.32f, 0.46f, 0.8f};
          for (int b = 0; b < 4; b++) {
            if (code[b] < 0) continue;
            float mx2 = mix(ax2, bx2, at[b]), my2 = mix(ay2, by2, at[b]);
            g.drawWideLine(mx2 - nx, my2 - ny, mx2 + nx, my2 + ny, half, col(theme.bands[code[b]]));
          }
        }
        break;
      }
      case PartKind::Led: {
        uint16_t c = col(it.partColor >= 0 ? it.partColor : theme.led);
        // Legs: the long one (+, leg 1) bends on its way up.
        float kx = mx + (p[0][0] - mx) * 0.25f, ky = my + (p[0][1] - my) * 0.25f;
        line3(g, cam, p[0][0], p[0][1], 0, p[0][0], p[0][1], 0.8f, lr, lead);
        line3(g, cam, p[0][0], p[0][1], 0.8f, kx, ky, lift, lr, lead);
        float qx = mx + (p[1][0] - mx) * 0.25f, qy = my + (p[1][1] - my) * 0.25f;
        line3(g, cam, p[1][0], p[1][1], 0, qx, qy, lift, lr, lead);
        float rx, ry, rd;
        cam.project(mx, my, lift, rx, ry, rd);
        float rad = std::max(3.0f, s * 1.0f);
        g.fillEllipse(rx, ry, rad * 1.15f, std::max(1.0f, rad * 1.15f * cam.sinPitch), shade(c, 0.3f));  // rim
        cam.project(mx, my, lift + 1.1f, rx, ry, rd);
        g.fillCircle(rx, ry, rad, c);
        g.fillCircle(rx - rad / 3, ry - rad / 3, std::max(1.0f, rad / 4), col(TFT_WHITE));
        if (s >= metrics.bbLabelPx * 0.8f || st == 2) {  // "+" by the long leg
          float ax2, ay2, ad;
          cam.project(p[0][0], p[0][1], 0.4f, ax2, ay2, ad);
          g.setFont(metrics.smallFont);
          g.setTextDatum(middle_center);
          g.setTextColor(st == 2 ? theme.accent : theme.text);
          g.drawString("+", ax2 + (ax2 < rx ? -6 : 6), ay2);
        }
        break;
      }
      default: {  // button, buzzer, box: a block between the legs
        line3(g, cam, p[0][0], p[0][1], 0, p[0][0], p[0][1], 0.5f, lr, lead);
        line3(g, cam, p[1][0], p[1][1], 0, p[1][0], p[1][1], 0.5f, lr, lead);
        float half = kind == PartKind::Buzzer ? 1.5f : std::max(1.0f, len / 2);
        float top = kind == PartKind::Buzzer ? 2.6f : 1.4f;
        uint16_t body = kind == PartKind::Button ? theme.buttonOff : kind == PartKind::Buzzer ? theme.buzzer : theme.bg;
        if (kind == PartKind::Box && it.partColor >= 0) body = it.partColor;
        box(g, cam, mx - half, mx + half, my - half, my + half, 0.5f, top, col(body));
        float tx, ty, td;
        cam.project(mx, my, top, tx, ty, td);
        if (kind == PartKind::Button) {
          uint16_t cap = col(it.partColor >= 0 ? it.partColor : theme.buttonCap);
          float rr = std::max(2.0f, s * half * 0.5f);
          g.fillEllipse(tx, ty, rr, std::max(1.0f, rr * cam.sinPitch), cap);
        } else if (kind == PartKind::Buzzer) {
          g.fillCircle(tx, ty, std::max(1.0f, s * 0.25f), col(TFT_BLACK));
        } else if (it.label.length() && s >= 8) {
          g.setFont(metrics.smallFont);
          g.setTextDatum(middle_center);
          g.setTextColor(col(detail::ink(body)));
          g.drawString(it.label, tx, ty);
        }
      }
    }
  }

  // ---- Places of the step: rings and names, blinking ----
  if (si >= 0) {
    g.setFont(metrics.smallFont);
    const int th = g.fontHeight() + 2;
    int lx[2], ly[2], tw[2];
    String names[2];
    bool ok[2] = {false, false};
    for (int k2 = 0; k2 < 2; k2++) {
      const End& e = items[si].ends[k2];
      float x, y, z, px, py, pd;
      if (!pos(e, x, y, z)) continue;
      cam.project(x, y, z, px, py, pd);
      int rr = std::max(4, (int)(s * 0.6f));
      if (phase) {
        g.drawCircle(px, py, rr, theme.accent);
        g.drawCircle(px, py, rr + 1, theme.accent);
      }
      String& name = names[k2];
      if (e.type == 3) name = "pin " + String(e.a) + (pins[e.a - 1].name.length() ? " " + pins[e.a - 1].name : String());
      else if (e.type == 2) name = String(e.b > 0 ? "+" : "-") + (abs(e.b) == 2 ? "a:" : ":") + String(e.a);
      else name = String(e.a) + (char)('a' + e.b);
      tw[k2] = g.textWidth(name) + 4;
      lx[k2] = (int)px - tw[k2] / 2;
      ly[k2] = (int)py - rr - th - 2;
      ok[k2] = true;
    }
    if (ok[0] && ok[1] && abs(ly[0] - ly[1]) < th) {  // side by side: push apart
      int l = lx[0] <= lx[1] ? 0 : 1, r = 1 - l, over = lx[l] + tw[l] + 2 - lx[r];
      if (over > 0) lx[l] -= (over + 1) / 2, lx[r] += (over + 1) / 2;
    }
    for (int k2 = 0; k2 < 2; k2++) {
      if (!ok[k2]) continue;
      int x = constrain(lx[k2], 0, w - tw[k2]), y = constrain(ly[k2], 0, h - th);
      g.fillRoundRect(x, y, tw[k2], th, 2, theme.accent);
      g.setTextColor(detail::ink(theme.accent));
      g.setTextDatum(top_left);
      g.drawString(names[k2], x + 2, y + 1);
    }
  }

  // ---- Header labels: pins in use, or all when close enough. Stacked so they do not overlap, with a line
  // back to the pin when moved. ----
  if (npins) {
    g.setFont(metrics.smallFont);
    const int fh = g.fontHeight();
    const bool all = s >= metrics.bbLabelPx;
    struct Label {
      float px, py, x, y;
      String text;
      bool left, used;
    };
    std::vector<Label> labels;
    for (int i = 0; i < npins; i++) {
      bool used = false;
      for (const Item& it : items)
        for (const End& e : it.ends) used = used || (e.type == 3 && e.a == i + 1);
      if ((!all && !used) || stepUses(3, i + 1, 0)) continue;  // the step's pins are labeled above
      float x, y, z;
      Place p = {3, (int16_t)(i + 1), 0};
      detail::placePosition(p, cols, npins, x, y, z);
      float px, py, pd, qx, qy, qd;
      cam.project(x, y, z, px, py, pd);
      if (px < 0 || px >= w || py < 0 || py >= h) continue;  // pin out of view
      cam.project(x + (i % 2 ? 1.3f : -1.3f), y, z, qx, qy, qd);
      bool left = qx < px;
      String t = pins[i].name.length() ? (left ? pins[i].name + " " + String(i + 1) : String(i + 1) + " " + pins[i].name) : String(i + 1);
      Label l = {px, py, qx, qy, t, left, used};
      labels.push_back(l);
    }
    for (int side = 0; side < 2; side++) {
      std::vector<Label*> col;
      for (Label& l : labels)
        if (l.left == (side == 0)) col.push_back(&l);
      std::sort(col.begin(), col.end(), [](const Label* a, const Label* b) { return a->y < b->y; });
      for (size_t i = 1; i < col.size(); i++) col[i]->y = std::max(col[i]->y, col[i - 1]->y + fh + 1);
    }
    for (const Label& l : labels) {
      int tw = g.textWidth(l.text);
      int x = l.left ? std::max((int)l.x, tw) : std::min((int)l.x, w - tw);
      if (fabsf(l.y - l.py) > 3 || fabsf(x - l.x) > 3) g.drawLine(l.px, l.py, x + (l.left ? 1 : -1), l.y, theme.dim);
      g.setTextDatum(l.left ? middle_right : middle_left);
      g.setTextColor(l.used ? theme.text : theme.dim);
      g.drawString(l.text, x, l.y);
    }
    // board name past the far end
    float bx, by, bd;
    cam.project(hx, rows / 2.0f + 2, 0, bx, by, bd);
    g.setTextDatum(bottom_center);
    g.setTextColor(theme.text);
    g.drawString(detail::fit(g, board, w / 2), bx, by);
  }

  // ---- Button: whole view / step ----
  int bs = std::max(20, h / 6), bx = w - bs / 2 - 4, by = h - bs / 2 - 4;
  g.fillCircle(bx, by, bs / 2, theme.buttonOff);
  g.drawCircle(bx, by - 1, bs / 4, theme.text);  // magnifier
  g.drawLine(bx + bs / 6, by + bs / 6 - 1, bx + bs / 3, by + bs / 3 - 1, theme.text);
  g.drawFastHLine(bx - bs / 8, by - 1, bs / 4 + 1, theme.text);
  if (!toStep) g.drawFastVLine(bx, by - 1 - bs / 8, bs / 4 + 1, theme.text);
  g.setTextDatum(top_left);
}

}  // namespace m5ui
