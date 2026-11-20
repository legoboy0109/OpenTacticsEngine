// Tiny self-registering test harness (no dependencies, no exceptions).
// Usage:  TEST(my_case) { CHECK(x); CHECK_EQ(a, b); }
// Run a subset:  ote_tests <substring-of-name>
#pragma once
#include <cstdio>
#include <cstdlib>
#include <string>

namespace ote_test {

struct Case {
  const char* name;
  void (*fn)();
  Case* next;
};
Case*& head();
int& failures();

struct Registrar {
  Registrar(const char* name, void (*fn)()) {
    // One small allocation per test, never freed: a fixed pool overflowed silently once the suite passed 1024 tests.
    Case* c = new Case;
    c->name = name;
    c->fn = fn;
    c->next = head();
    head() = c;
  }
};

// CHECK passes its condition through this so a condition made only of constants (an enum compared to a number) does not
// trip MSVC's "conditional expression is constant" warning; the check still runs.
inline bool truth(bool value) { return value; }

// Root of the source tree, for locating games/ and tests/fixtures/.
inline std::string sourceDir() { return OTE_SOURCE_DIR; }
// Scratch folder for tests that write files: <build>/test-tmp/ote_tests (OTE_TEST_TMP, which CMake sets for ctest, replaces
// <build>/test-tmp). Never inside the source tree. test_main.cpp wipes and creates it before the run and removes it when
// every case passed; the Python tests use <same root>/<script name> (tests/ote_testkit.py scratch_dir).
inline std::string tempDir() {
  static const std::string dir = [] {
    const char* root = std::getenv("OTE_TEST_TMP");
    return std::string(root && *root ? root : OTE_TEMP_DIR "/test-tmp") + "/ote_tests";
  }();
  return dir;
}

}  // namespace ote_test

#define TEST(name)                                              \
  static void name();                                           \
  static ::ote_test::Registrar ote_reg_##name(#name, name);     \
  static void name()

#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!::ote_test::truth(cond)) {                                        \
      std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
      ++::ote_test::failures();                                            \
    }                                                                      \
  } while (0)

#define CHECK_EQ(a, b)                                                     \
  do {                                                                     \
    auto ote_a = (a);                                                      \
    auto ote_b = (b);                                                      \
    if (!(ote_a == ote_b)) {                                               \
      std::printf("  FAIL %s:%d: %s == %s (%s vs %s)\n", __FILE__,         \
                  __LINE__, #a, #b, std::to_string(ote_a).c_str(),         \
                  std::to_string(ote_b).c_str());                          \
      ++::ote_test::failures();                                            \
    }                                                                      \
  } while (0)

#define CHECK_STR(a, b)                                                    \
  do {                                                                     \
    std::string ote_a = (a);                                               \
    std::string ote_b = (b);                                               \
    if (ote_a != ote_b) {                                                  \
      std::printf("  FAIL %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__,  \
                  ote_a.c_str(), ote_b.c_str());                           \
      ++::ote_test::failures();                                            \
    }                                                                      \
  } while (0)
