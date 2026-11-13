#include "core/base/log.h"

#include <cstdarg>
#include <cstdio>

namespace ote {

static void defaultSink(LogLevel level, const char* message) {
  static const char* names[] = {"debug", "info", "warn", "ERROR"};
  std::fprintf(stderr, "[%s] %s\n", names[(int)level], message);
}

static LogSink g_sink = defaultSink;
static LogLevel g_min = LogLevel::Info;

void setLogSink(LogSink sink) { g_sink = sink ? sink : defaultSink; }
void setLogLevel(LogLevel minimum) { g_min = minimum; }

void logf(LogLevel level, const char* fmt, ...) {
  if ((int)level < (int)g_min) return;
  char buf[1024];
  va_list args;
  va_start(args, fmt);
  std::vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  g_sink(level, buf);
}

}  // namespace ote
