#include "m5ui.h"

#include <stdarg.h>
#include <time.h>

namespace m5ui {
namespace events {

static String lines[KEEP];
static size_t head = 0, n = 0;

void add(const char* fmt, ...) {
  char msg[160];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);
  Serial.println(msg);
  char stamp[16];
  time_t t = time(nullptr);
  if (t > 1735689600) {  // clock is set (after 2025-01-01)
    struct tm lt;
    localtime_r(&t, &lt);
    strftime(stamp, sizeof stamp, "%H:%M:%S", &lt);
  } else {
    snprintf(stamp, sizeof stamp, "+%lus", (unsigned long)(millis() / 1000));
  }
  lines[head] = String(stamp) + " " + msg;
  head = (head + 1) % KEEP;
  if (n < KEEP) n++;
}

size_t count() { return n; }

const String& at(size_t i) { return lines[(head + KEEP - 1 - i) % KEEP]; }

}  // namespace events
}  // namespace m5ui
