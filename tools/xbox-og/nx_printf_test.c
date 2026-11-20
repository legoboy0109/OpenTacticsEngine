// Host test of nx_printf.c against the host's printf. Not part of any build; run by hand:
//   gcc -O1 -DNX_PRINTF_TEST -o /tmp/nx_printf_test tools/xbox-og/nx_printf_test.c tools/xbox-og/nx_printf.c && /tmp/nx_printf_test
// Prints the mismatches and a count; exit status 1 if any value is off by more than the last printed digit.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int nx_snprintf(char* buf, size_t size, const char* fmt, ...);

static int bad = 0, total = 0;

static void check(const char* fmt, double v) {
  char a[512], b[512];
  snprintf(a, sizeof a, fmt, v);
  nx_snprintf(b, sizeof b, fmt, v);
  ++total;
  if (strcmp(a, b) == 0) return;
  // The only tolerated difference: the last significant digit of a %.17g-style number (a power of ten built by repeated
  // multiplication). Compare by value instead.
  char* e1;
  char* e2;
  double x = strtod(a, &e1), y = strtod(b, &e2);
  if (*e1 == 0 && *e2 == 0 && fabs(x - y) <= fabs(x) * 2e-16) return;
  if (bad < 40) printf("MISMATCH %-8s %.17g: host '%s' ours '%s'\n", fmt, v, a, b);
  ++bad;
}

int main(void) {
  const char* fmts[] = {"%.0f", "%.1f", "%.2f", "%f", "%8.3f", "%-8.2f|", "%+.1f", "%08.2f", "%g", "%.3g", "%.6g", "%.15g", "%.17g",
                        "%e", "%.2e", "%.0e", "%10.4g", "%G", "%#g", "%#.0f"};
  const double vals[] = {0, -0.0, 1, -1, 0.5, 1.5, 2.5, 0.1, 0.2, 0.3, 1.0 / 3, 2.0 / 3, 3.14159265358979, 100, 1234567, 1e15, 1e-5,
                         1.5e-7, 123456789.125, 0.000123456, 99.995, 9.9999999, 0.045, 1e10, 123456.789, 5e-324 * 0 + 1e-300, 1e100,
                         12345678901234567.0, 0.7, 7.77, 1e21, 255.0 / 256};
  for (size_t i = 0; i < sizeof fmts / sizeof *fmts; ++i)
    for (size_t j = 0; j < sizeof vals / sizeof *vals; ++j) check(fmts[i], vals[j]);
  srand(7);
  for (int i = 0; i < 20000; ++i) {
    double v = (double)rand() / RAND_MAX * pow(10.0, rand() % 20 - 8);
    check("%.17g", v);
    check("%.2f", v);
    check("%g", v);
    check("%.1f", v);
  }
  // Integers, strings and mixed formats go through the same function.
  char a[256], b[256];
  const char* mixed = "x=%d u=%u l=%ld ll=%lld z=%zu s=%s [%5s] [%-5s] c=%c h=%x %%";
  snprintf(a, sizeof a, mixed, -5, 7u, -8L, 9LL, (size_t)10, "str", "ab", "ab", 'q', 255);
  nx_snprintf(b, sizeof b, mixed, -5, 7u, -8L, 9LL, (size_t)10, "str", "ab", "ab", 'q', 255);
  ++total;
  if (strcmp(a, b)) { printf("MISMATCH mixed: '%s' vs '%s'\n", a, b); ++bad; }
  // snprintf's contract: the length that would have been written, and truncation with a terminator.
  char small[8];
  int n = nx_snprintf(small, sizeof small, "%.3f and more", 1.25);
  ++total;
  if (n != 14 || strcmp(small, "1.250 a") != 0) { printf("MISMATCH truncation: %d '%s'\n", n, small); ++bad; }
  printf("%d checks, %d mismatches\n", total, bad);
  return bad != 0;
}
