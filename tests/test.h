// Tiny self-registering test harness (no dependencies, no exceptions).
// Usage:  TEST(my_case) { CHECK(x); CHECK_EQ(a, b); }
// Run a subset:  ote_tests <substring-of-name>
#pragma once
#include <cstdio>
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
    static Case pool[1024];
    static int used = 0;
    Case* c = &pool[used++];
    c->name = name;
    c->fn = fn;
    c->next = head();
    head() = c;
  }
};

// Root of the source tree, for locating games/ and tests/fixtures/.
inline std::string sourceDir() { return OTE_SOURCE_DIR; }

}  // namespace ote_test

#define TEST(name)                                              \
  static void name();                                           \
  static ::ote_test::Registrar ote_reg_##name(#name, name);     \
  static void name()

#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
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
