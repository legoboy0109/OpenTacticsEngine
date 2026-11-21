// Runner of the Qt-free editor-core unit tests (schema engine, id sources, and the pieces of ote-mcp that need no I/O).
// Same harness as ote_tests (tests/test.h); no scratch folder is needed, so no shared one is touched.
//   ote_editor_core_tests [substring-of-test-name]
#include <cstring>

#include "test.h"

namespace ote_test {
Case*& head() { static Case* h = nullptr; return h; }
int& failures() { static int f = 0; return f; }
}  // namespace ote_test

int main(int argc, char** argv) {
  const char* filter = argc > 1 ? argv[1] : nullptr;
  int ran = 0, failedCases = 0;
  for (ote_test::Case* c = ote_test::head(); c; c = c->next) {
    if (filter && !std::strstr(c->name, filter)) continue;
    const int before = ote_test::failures();
    c->fn();
    ++ran;
    if (ote_test::failures() != before) {
      ++failedCases;
      std::printf("FAILED: %s\n", c->name);
    }
  }
  std::printf("%d test(s) run, %d failed\n", ran, failedCases);
  return failedCases ? 1 : 0;
}
