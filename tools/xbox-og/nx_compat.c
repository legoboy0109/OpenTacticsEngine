// nxdk C-library gaps the engine hits. nxdk's strtod is a stub that asserts ("Assertion failed: '0' in function strtod"),
// but the JSON loader and Lua both parse numbers with it. The Makefiles compile everything with -Dstrtod=ote_strtod, so
// this definition (named strtod here, renamed by that flag) replaces the call everywhere without clashing with libxboxrt.
// Exact (correctly rounded) when the digits fit 2^53 and the decimal exponent is within +-22, otherwise approximate.
#include <stddef.h>
#include "nx_compat.h"

double strtod(const char* s, char** end) {
  const char* p = s;
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\v' || *p == '\f') ++p;
  int neg = 0;
  if (*p == '-') { neg = 1; ++p; } else if (*p == '+') ++p;
  double mant = 0;
  int exp10 = 0, digits = 0;
  const char* numStart = p;
  while (*p >= '0' && *p <= '9') { if (digits < 18) { mant = mant * 10 + (*p - '0'); if (mant > 0) ++digits; } else ++exp10; ++p; }
  if (*p == '.') {
    ++p;
    while (*p >= '0' && *p <= '9') { if (digits < 18) { mant = mant * 10 + (*p - '0'); if (mant > 0) ++digits; --exp10; } ++p; }
  }
  if (p == numStart || (p == numStart + 1 && *numStart == '.')) { if (end) *end = (char*)s; return 0.0; }
  if (*p == 'e' || *p == 'E') {
    const char* q = p + 1;
    int eneg = 0, e = 0, any = 0;
    if (*q == '-') { eneg = 1; ++q; } else if (*q == '+') ++q;
    while (*q >= '0' && *q <= '9') { if (e < 10000) e = e * 10 + (*q - '0'); any = 1; ++q; }
    if (any) { exp10 += eneg ? -e : e; p = q; }
  }
  double v = mant;
  if (v != 0.0) {
    int e = exp10;
    while (e > 0) { int step = e > 22 ? 22 : e; double f = 1; for (int i = 0; i < step; ++i) f *= 10; v *= f; e -= step; if (v > 1e308) { v = 1e308 * 10; break; } }
    while (e < 0) { int step = -e > 22 ? 22 : -e; double f = 1; for (int i = 0; i < step; ++i) f *= 10; v /= f; e += step; if (v == 0.0) break; }
  }
  if (end) *end = (char*)p;
  return neg ? -v : v;
}

// nxdk's lround/llround/log2 are stubs that assert too. -Dlround=ote_lround etc. (see the Makefiles) route calls here.
#include <math.h>
long lround(double x) { return (long)(x < 0 ? x - 0.5 : x + 0.5); }
long long llround(double x) { return (long long)(x < 0 ? x - 0.5 : x + 0.5); }
long lroundf(float x) { return (long)(x < 0 ? x - 0.5f : x + 0.5f); }
long long llroundf(float x) { return (long long)(x < 0 ? x - 0.5f : x + 0.5f); }
double log2(double x) { return log(x) * 1.4426950408889634; }
long lrint(double x) { return (long)(x < 0 ? x - 0.5 : x + 0.5); }  // ties go away from zero, not to even: fine for this engine
long long llrint(double x) { return (long long)(x < 0 ? x - 0.5 : x + 0.5); }
long lrintf(float x) { return (long)(x < 0 ? x - 0.5f : x + 0.5f); }
long long llrintf(float x) { return (long long)(x < 0 ? x - 0.5f : x + 0.5f); }
double exp2(double x) { return pow(2.0, x); }
float exp2f(float x) { return (float)pow(2.0, x); }
double cbrt(double x) { return x < 0 ? -pow(-x, 1.0 / 3.0) : pow(x, 1.0 / 3.0); }
double log1p(double x) { return log(1.0 + x); }
double expm1(double x) { return exp(x) - 1.0; }
double scalbn(double x, int n) { return ldexp(x, n); }
double fdim(double x, double y) { return x > y ? x - y : 0.0; }

// getenv / setenv: nxdk has no environment. Called as getenv everywhere (renamed by nx_compat.h); ote_setenv is ours.
#include <stdlib.h>
#include <string.h>
enum { kMaxEnv = 64 };
static char* g_envName[kMaxEnv];
static char* g_envValue[kMaxEnv];
char* getenv(const char* name) {
  for (int i = 0; i < kMaxEnv; ++i)
    if (g_envName[i] && strcmp(g_envName[i], name) == 0) return g_envValue[i];
  return NULL;
}
int ote_setenv(const char* name, const char* value) {
  int slot = -1;
  for (int i = 0; i < kMaxEnv; ++i) {
    if (g_envName[i] && strcmp(g_envName[i], name) == 0) { slot = i; break; }
    if (!g_envName[i] && slot < 0) slot = i;
  }
  if (slot < 0) return -1;
  char* v = (char*)malloc(strlen(value) + 1);
  if (!v) return -1;
  strcpy(v, value);
  if (!g_envName[slot]) {
    g_envName[slot] = (char*)malloc(strlen(name) + 1);
    if (!g_envName[slot]) { free(v); return -1; }
    strcpy(g_envName[slot], name);
  } else {
    free(g_envValue[slot]);
  }
  g_envValue[slot] = v;
  return 0;
}
