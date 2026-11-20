#include <cstring>
#include <filesystem>
#include <system_error>

#include "test.h"

namespace ote_test {
Case*& head() { static Case* h = nullptr; return h; }
int& failures() { static int f = 0; return f; }
}  // namespace ote_test

int main(int argc, char** argv) {
  const char* filter = argc > 1 ? argv[1] : nullptr;
  // A fresh scratch folder (tests/test.h tempDir): a leftover of an earlier failed run must not be seen by this one.
  // error_code overloads: the engine builds without exceptions.
  std::error_code ec;
  std::filesystem::remove_all(ote_test::tempDir(), ec);
  std::filesystem::create_directories(ote_test::tempDir(), ec);
  int ran = 0, failedCases = 0;
  for (ote_test::Case* c = ote_test::head(); c; c = c->next) {
    if (filter && !std::strstr(c->name, filter)) continue;
    int before = ote_test::failures();
    c->fn();
    ++ran;
    if (ote_test::failures() != before) {
      ++failedCases;
      std::printf("FAILED: %s\n", c->name);
    }
  }
  std::printf("%d test(s) run, %d failed\n", ran, failedCases);
  if (!failedCases) std::filesystem::remove_all(ote_test::tempDir(), ec);  // kept after a failure, to look at
  return failedCases ? 1 : 0;
}
