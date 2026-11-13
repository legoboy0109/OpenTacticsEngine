// stderr logging for the helper, plus the bridge from the engine's global
// log sink. Only touched while the API mutex is held (or at startup).
#pragma once
#include <string>
#include <vector>

namespace ote_editor {

void elog(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// Routes engine Warn/Error messages to stderr.
void installEngineLogSink();

// While alive, engine warnings/errors are also collected here, so validation
// can report e.g. a Lua runtime error that the engine only logs.
class EngineLogCapture {
 public:
  EngineLogCapture();
  ~EngineLogCapture();
  EngineLogCapture(const EngineLogCapture&) = delete;
  EngineLogCapture& operator=(const EngineLogCapture&) = delete;
  std::vector<std::string> lines;
};

}  // namespace ote_editor
