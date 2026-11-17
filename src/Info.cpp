#include <algorithm>

#include "m5ui_internal.h"

namespace m5ui {

using detail::fit;
using detail::makeSprite;
using detail::wrap;

// ---- InfoView ----

bool InfoView::begin() {
  const Metrics& m = metrics;
  bool corner = m.cornerW > 0 && m.cornerH > 0;
  int bandH = corner ? std::max(m.cornerH, m.titleH) : m.titleH;
  headX = m.infoX + (corner ? m.cornerW : 0);
  headY = m.infoY;
  bodyX = m.infoX;
  bodyY = m.infoY + bandH;
  headDirty = bodyDirty = true;
  bool a = makeSprite(head, m.infoW - (headX - m.infoX), bandH);
  bool b = makeSprite(body, m.infoW, m.infoH - bandH);
  return a && b;
}

void InfoView::open(const String& t, Content* c) {
  if (!body.getBuffer() || !head.getBuffer()) begin();
  title = t;
  content = c;
  opened = true;
  show();
}

void InfoView::setTitle(const String& t) {
  if (t == title) return;
  title = t;
  headDirty = true;
}

void InfoView::setContent(Content* c) {
  content = c;
  bodyDirty = true;
}

void InfoView::close() { opened = false; }

void InfoView::show() {
  headDirty = bodyDirty = true;
  update();
}

void InfoView::update() {
  if (!opened) return;
  if (detail::damaged(damageSeen, {bodyX, headY, body.width(), bodyY + body.height() - headY}))
    headDirty = bodyDirty = true;
  bool moved = content && content->animate(millis());
  if (headDirty) drawHead();
  if (bodyDirty || moved || (content && content->dirty)) drawBody();
}

void InfoView::drawHead() {
  headDirty = false;
  if (!head.getBuffer()) return;
  int pad = metrics.pad;
  head.fillScreen(theme.screen);
  head.setFont(metrics.titleFont);
  head.setTextColor(theme.text);
  head.setTextDatum(middle_left);
  int titleW = head.width() - pad * 2;
  const Metrics& m = metrics;  // leave room for a badge in the title band
  if (m.badgeSize > 0 && m.badgeX < headX + head.width() && m.badgeX + m.badgeSize > headX &&
      m.badgeY < headY + head.height() && m.badgeY + m.badgeSize > headY)
    titleW = std::min(titleW, m.badgeX - headX - pad * 2);
  head.drawString(fit(head, title, titleW), pad, head.height() / 2);
  head.drawFastHLine(pad, head.height() - 1, head.width() - pad * 2, theme.line);
  detail::push(head, headX, headY);
}

void InfoView::drawBody() {
  bodyDirty = false;
  if (content) content->dirty = false;
  if (!body.getBuffer()) return;
  body.fillScreen(theme.screen);
  body.setFont(metrics.font);
  body.setTextColor(theme.text);
  body.setTextDatum(top_left);
  // The content draws from the top, then moves down below the gap under the title band's line.
  const int g = gap();
  if (content) content->draw(body, body.width(), body.height() - g);
  if (g) {
    body.setBaseColor(theme.screen);
    body.scroll(0, g);
  }
  detail::push(body, bodyX, bodyY);
}

// ---- Terminal ----

void Terminal::print(const String& text, Level level) {
  int start = 0, n = text.length();
  for (;;) {
    int end = text.indexOf('\n', start);
    if (end < 0) end = n;
    String s = text.substring(start, end);
    s.replace("\r", "");
    lines.push_back({s, level});
    if (end >= n) break;
    start = end + 1;
  }
  if (lines.size() > keep) lines.erase(lines.begin(), lines.end() - keep);
  invalidate();
}

void Terminal::clear() {
  lines.clear();
  invalidate();
}

void Terminal::setWrap(bool on) {
  wrapLines = on;
  invalidate();
}

void Terminal::draw(LovyanGFX& g, int w, int h) {
  int pad = metrics.pad, lineH = g.fontHeight() + metrics.lineGap, tw = w - pad * 2;
  size_t maxLines = std::max(1, (h - pad * 2 + metrics.lineGap) / lineH);
  // Newest first, until the view is full.
  std::vector<String> parts, shown;
  std::vector<Level> levels;
  for (int i = (int)lines.size() - 1; i >= 0 && shown.size() < maxLines; i--) {
    parts.clear();
    if (wrapLines) wrap(g, lines[i].text, tw, parts);
    else parts.push_back(fit(g, lines[i].text, tw));
    for (int k = (int)parts.size() - 1; k >= 0 && shown.size() < maxLines; k--) {
      shown.push_back(parts[k]);
      levels.push_back(lines[i].level);
    }
  }
  int y = pad;
  for (int k = (int)shown.size() - 1; k >= 0; k--, y += lineH) {
    Level l = levels[k];
    g.setTextColor(l == Level::Error ? theme.error : l == Level::Warn ? theme.warn : theme.text);
    g.drawString(shown[k], pad, y);
  }
}

// ---- Typing ----

static const size_t TYPING_KEEP = 2048;

void Typing::trim() {
  if (typed.length() <= TYPING_KEEP) return;
  size_t cut = typed.length() - TYPING_KEEP;
  while (cut < typed.length() && ((uint8_t)typed[cut] & 0xC0) == 0x80) cut++;  // whole characters only
  typed.remove(0, cut);
}

void Typing::setText(const String& t) {
  if (t == typed) return;
  typed = t;
  trim();
  invalidate();
}

void Typing::append(const String& more) {
  if (!more.length()) return;
  typed += more;
  trim();
  invalidate();
}

void Typing::setProgress(int d, int t) {
  if (d == done && t == total) return;
  done = d;
  total = t;
  invalidate();
}

void Typing::clear() {
  typed = "";
  done = total = 0;
  invalidate();
}

bool Typing::animate(uint32_t now) {
  bool p = timing.cursorMs ? (now / timing.cursorMs) % 2 == 0 : true;
  if (p == phase) return false;
  phase = p;
  return true;
}

void Typing::draw(LovyanGFX& g, int w, int h) {
  int pad = metrics.pad, fh = g.fontHeight(), lineH = fh + metrics.lineGap, top = pad;
  if (total > 0) {
    String label = String(done) + " / " + String(total);
    int barW = w - pad * 3 - g.textWidth(label), barH = std::max(4, fh / 2), by = top + (fh - barH) / 2;
    g.fillRoundRect(pad, by, barW, barH, barH / 2, theme.buttonOff);
    int fw = (int)((int64_t)barW * constrain(done, 0, total) / total);
    if (fw > 0) g.fillRoundRect(pad, by, std::max(fw, barH), barH, barH / 2, theme.accent);
    g.setTextColor(theme.dim);
    g.setTextDatum(top_right);
    g.drawString(label, w - pad, top);
    g.setTextDatum(top_left);
    top += lineH + pad;
  }
  int tw = w - pad * 2, cw = std::max(2, fh / 2);
  std::vector<String> out;
  wrap(g, typed, tw, out);
  int lastW = g.textWidth(out.back());
  if (lastW + cw > tw) {  // no room for the cursor
    out.push_back(String());
    lastW = 0;
  }
  int maxLines = std::max(1, (h - top - pad + metrics.lineGap) / lineH);
  int first = std::max(0, (int)out.size() - maxLines), y = top;
  g.setTextColor(theme.text);
  for (int i = first; i < (int)out.size(); i++, y += lineH) g.drawString(out[i], pad, y);
  if (phase) g.fillRect(pad + lastW + 1, y - lineH, cw, fh, theme.accent);
}

// ---- StatusList ----

void StatusList::set(const String& name, const String& value, Light light) {
  for (Row& r : rows) {
    if (r.name != name) continue;
    if (r.value == value && r.light == light) return;
    r.value = value;
    r.light = light;
    invalidate();
    return;
  }
  rows.push_back({name, value, light});
  invalidate();
}

void StatusList::remove(const String& name) {
  for (size_t i = 0; i < rows.size(); i++) {
    if (rows[i].name != name) continue;
    rows.erase(rows.begin() + i);
    invalidate();
    return;
  }
}

void StatusList::clear() {
  rows.clear();
  invalidate();
}

void StatusList::draw(LovyanGFX& g, int w, int h) {
  int pad = metrics.pad, fh = g.fontHeight(), rowH = std::max<int>(metrics.rowH, fh + 4);
  int n = rows.size(), perCol = std::max(1, (h - pad * 2) / rowH);
  int cols = n > perCol && w >= 240 ? 2 : 1;
  int colW = (w - pad * 2 - pad * 2 * (cols - 1)) / cols, dot = std::max(3, fh / 3);
  for (int i = 0; i < n && i / perCol < cols; i++) {
    const Row& r = rows[i];
    int x = pad + (i / perCol) * (colW + pad * 2), cy = pad + (i % perCol) * rowH + rowH / 2;
    g.fillCircle(x + dot, cy, dot, detail::lightColor(r.light));
    int nameX = x + dot * 2 + 6;
    String value = fit(g, r.value, (x + colW - nameX) * 2 / 3);
    int vw = g.textWidth(value);
    g.setTextDatum(middle_left);
    g.setTextColor(theme.dim);
    g.drawString(fit(g, r.name, x + colW - nameX - vw - 6), nameX, cy);
    g.setTextDatum(middle_right);
    g.setTextColor(theme.text);
    g.drawString(value, x + colW, cy);
  }
  g.setTextDatum(top_left);
}

// ---- Subtitle ----

bool Subtitle::ready() { return makeSprite(canvas, M5.Display.width(), metrics.subtitleH); }

enum { HOLD_START, SCROLL, HOLD_END };

int Subtitle::over() const { return std::max(0, textW - (canvas.width() - metrics.pad * 2)); }

void Subtitle::set(const String& text) {
  if (text == txt && canvas.getBuffer()) return;
  if (!ready()) return;
  bool more = txt.length() && text.length() > txt.length() && text.startsWith(txt);
  txt = text;
  canvas.setFont(metrics.font);
  textW = canvas.textWidth(txt);
  uint32_t now = millis();
  if (!more) {  // a new text: from its start, or at its end when keeping the end in view
    pos = metrics.subtitleTail ? over() : 0;
    phase = HOLD_START;
    phaseAt = now;
  } else if (phase == HOLD_END && over() > pos) {  // more to scroll: go on from here
    phase = SCROLL;
  }
  lastAt = now;
  draw((int)pos);
}

void Subtitle::advance(uint32_t now) {
  float dt = now - lastAt, speed = timing.subtitleSpeed;
  lastAt = now;
  int o = over();
  if (!o) {
    pos = 0;
    return;
  }
  if (metrics.subtitleTail) {  // chase the end: at least the speed, faster when far behind
    float step = speed ? std::max(speed * dt / 1000, (o - pos) * std::min(1.0f, dt / 150)) : o;
    pos = std::min((float)o, pos + step);
    return;
  }
  if (!speed) {
    pos = 0;
    return;
  }
  uint32_t hold = timing.subtitleHoldMs;
  if (phase == HOLD_START && now - phaseAt >= hold) {
    phase = SCROLL;
  } else if (phase == SCROLL) {
    pos += speed * dt / 1000;
    if (pos >= o) pos = o, phase = HOLD_END, phaseAt = now;
  } else if (phase == HOLD_END && now - phaseAt >= hold) {
    pos = 0, phase = HOLD_START, phaseAt = now;
  }
}

void Subtitle::update() {
  if (!canvas.getBuffer() || !txt.length()) return;
  advance(millis());
  if ((int)pos != drawnOffset) draw((int)pos);
}

void Subtitle::show() {
  if (ready()) draw((int)pos);
}

void Subtitle::draw(int off) {
  drawnOffset = off;
  int pad = metrics.pad, w = canvas.width(), h = canvas.height(), avail = w - pad * 2;
  canvas.fillScreen(theme.subtitleBg);
  if (txt.length()) {
    canvas.setFont(metrics.font);
    canvas.setTextColor(theme.subtitle);
    if (textW <= avail && metrics.subtitleAlign == Align::Center) {
      canvas.setTextDatum(middle_center);
      canvas.drawString(txt, w / 2, h / 2);
    } else if (textW <= avail) {
      canvas.setTextDatum(middle_left);
      canvas.drawString(txt, pad, h / 2);
    } else if (!timing.subtitleSpeed && !metrics.subtitleTail) {
      canvas.setTextDatum(middle_left);
      canvas.drawString(fit(canvas, txt, avail), pad, h / 2);
    } else {
      canvas.setClipRect(pad, 0, avail, h);
      canvas.setTextDatum(middle_left);
      canvas.drawString(txt, pad - off, h / 2);
      canvas.clearClipRect();
    }
  }
  // While it shows text, the band is kept clear of InfoView and Face.
  detail::reserved[detail::RESERVED_SUBTITLE] = {0, metrics.subtitleY, txt.length() ? w : 0, h};
  detail::push(canvas, 0, metrics.subtitleY, detail::RESERVED_SUBTITLE);
}

}  // namespace m5ui
