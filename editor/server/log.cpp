#include "log.h"

#include <cstdarg>
#include <cstdio>

#include "core/base/log.h"

namespace ote_editor {

static EngineLogCapture* g_capture = nullptr;

void elog(const char* fmt, ...) {
  char buf[2048];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  std::fprintf(stderr, "[ote-editor] %s\n", buf);
  std::fflush(stderr);
}

static void engineSink(ote::LogLevel level, const char* message) {
  if (level < ote::LogLevel::Warn) return;
  elog("engine %s: %s", level == ote::LogLevel::Warn ? "warning" : "error", message);
  if (g_capture) g_capture->lines.push_back(message);
}

void installEngineLogSink() {
  ote::setLogSink(engineSink);
  ote::setLogLevel(ote::LogLevel::Warn);
}

EngineLogCapture::EngineLogCapture() { g_capture = this; }
EngineLogCapture::~EngineLogCapture() { g_capture = nullptr; }

}  // namespace ote_editor
