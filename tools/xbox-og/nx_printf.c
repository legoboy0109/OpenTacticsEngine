// printf with floating point for the nxdk build. nxdk's C library (pdclib) prints NOTHING for %f / %e / %g, so without this
// every number the engine writes as a decimal fraction or through "%.0f" (all of the JSON writer: saves, settings) came out
// empty ("saveVersion":,) and the Continue save was unreadable. nx_compat.h renames snprintf / vsnprintf to the functions
// below in every nxdk build unit; they handle the conversions themselves (%f %F %e %E %g %G) and hand everything else
// (integers, characters, pointers) to the C library one conversion at a time.
//
// The digits come from long double arithmetic (x87, 64-bit mantissa), correctly rounded for the 17 significant digits the
// engine ever asks for (%.17g, %.0f of whole numbers, %.1f ...) except in the last place for values far from 1 (a power of
// ten is built by repeated multiplication). tools/xbox-og/nx_printf_test.c compares it with the host's printf.
#include <stdarg.h>
#include <stddef.h>
#include <string.h>

// The C library's own functions, under their real names (nx_compat.h renamed the standard names for this file too).
#undef snprintf
#undef vsnprintf
int snprintf(char* buf, size_t size, const char* fmt, ...);

#ifndef NX_PRINTF_TEST
#define OTE_SNPRINTF ote_snprintf
#define OTE_VSNPRINTF ote_vsnprintf
#else
#define OTE_SNPRINTF nx_snprintf
#define OTE_VSNPRINTF nx_vsnprintf
#endif

enum { kFlagMinus = 1, kFlagPlus = 2, kFlagSpace = 4, kFlagHash = 8, kFlagZero = 16 };

// Appends n characters to the output (as many as fit; the total is still counted, as snprintf does).
static void emit(char* out, size_t cap, size_t* len, const char* s, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    if (*len + 1 < cap) out[*len] = s[i];
    ++*len;
  }
}

static void emitRepeat(char* out, size_t cap, size_t* len, char c, int n) {
  for (int i = 0; i < n; ++i) emit(out, cap, len, &c, 1);
}

// a > 0, finite. Fills digs[0..ndig) with the first ndig significant decimal digits of a, rounded, and sets *exp10 to the
// decimal exponent of the first digit (a = d.ddd * 10^exp10). ndig <= 18.
static void digitsOf(long double a, int ndig, int* exp10, char* digs) {
  int e = 0;
  long double t = a;
  while (t >= 1e16L) { t /= 1e16L; e += 16; }
  while (t >= 10.0L) { t /= 10.0L; ++e; }
  while (t < 1e-15L) { t *= 1e16L; e -= 16; }
  while (t < 1.0L) { t *= 10.0L; --e; }
  long double m = t;
  for (int i = 1; i < ndig; ++i) m *= 10.0L;
  unsigned long long n = (unsigned long long)m;
  const long double fr = m - (long double)n;  // round half to even, like glibc (exact ties are common: 0.5, 2.5, x.125)
  if (fr > 0.5L || (fr == 0.5L && (n & 1ull))) ++n;
  unsigned long long limit = 1;
  for (int i = 0; i < ndig; ++i) limit *= 10ull;
  if (n >= limit) {  // 9.99.. rounded up to 10.0..
    n /= 10ull;
    ++e;
  }
  for (int i = ndig - 1; i >= 0; --i) {
    digs[i] = (char)('0' + (int)(n % 10ull));
    n /= 10ull;
  }
  *exp10 = e;
}

// Formats one floating-point conversion into body (no sign, no padding); returns its length.
static int floatBody(char* body, size_t cap, long double a, char conv, int prec, int hash) {
  const int upper = conv == 'E' || conv == 'G' || conv == 'F';
  char lc = (char)(conv | 0x20);
  size_t n = 0;
  if (a != a) { memcpy(body, upper ? "NAN" : "nan", 3); return 3; }
  if (a > 1e4000L) { memcpy(body, upper ? "INF" : "inf", 3); return 3; }
  if (prec < 0) prec = 6;
  char digs[24];
  int exp10 = 0, ndig;
  int useE;
  if (lc == 'g') {
    int P = prec == 0 ? 1 : prec;
    if (P > 18) P = 18;
    ndig = P;
    if (a == 0) exp10 = 0, memset(digs, '0', (size_t)ndig);
    else digitsOf(a, ndig, &exp10, digs);
    useE = exp10 < -4 || exp10 >= P;
    // %g: P significant digits, trailing zeros dropped unless '#'
    int keep = ndig;
    if (!hash)
      while (keep > 1 && digs[keep - 1] == '0') --keep;
    if (useE) {
      body[n++] = digs[0];
      if (keep > 1 || hash) body[n++] = '.';
      for (int i = 1; i < keep; ++i) body[n++] = digs[i];
    } else if (exp10 >= 0) {
      for (int i = 0; i <= exp10; ++i) body[n++] = i < ndig ? digs[i] : '0';
      if (keep > exp10 + 1 || hash) body[n++] = '.';
      for (int i = exp10 + 1; i < keep; ++i) body[n++] = digs[i];
    } else {
      body[n++] = '0';
      body[n++] = '.';
      for (int i = 0; i < -exp10 - 1; ++i) body[n++] = '0';
      for (int i = 0; i < keep; ++i) body[n++] = digs[i];
    }
  } else if (lc == 'e') {
    ndig = prec + 1;
    if (ndig > 18) ndig = 18;
    if (a == 0) exp10 = 0, memset(digs, '0', (size_t)ndig);
    else digitsOf(a, ndig, &exp10, digs);
    useE = 1;
    body[n++] = digs[0];
    if (prec > 0 || hash) body[n++] = '.';
    for (int i = 1; i <= prec; ++i) body[n++] = i < ndig ? digs[i] : '0';
  } else {  // f
    long double scale = 1.0L;
    for (int i = 0; i < prec && i < 18; ++i) scale *= 10.0L;
    long double scaled = a * scale;
    if (prec <= 18 && scaled < 1.8e19L) {
      unsigned long long N = (unsigned long long)scaled;
      const long double fr = scaled - (long double)N;
      if (fr > 0.5L || (fr == 0.5L && (N & 1ull))) ++N;
      char tmp[24];
      int tl = 0;
      do { tmp[tl++] = (char)('0' + (int)(N % 10ull)); N /= 10ull; } while (N);
      while (tl < prec + 1) tmp[tl++] = '0';  // at least one digit before the point
      for (int i = tl - 1; i >= prec; --i) body[n++] = tmp[i];
      if (prec > 0 || hash) body[n++] = '.';
      for (int i = prec - 1; i >= 0; --i) body[n++] = tmp[i];
    } else {  // huge: 17 significant digits, then zeros
      digitsOf(a, 17, &exp10, digs);
      for (int i = 0; i <= exp10; ++i) body[n++] = i < 17 ? digs[i] : '0';
      if (prec > 0 || hash) body[n++] = '.';
      for (int i = 0; i < prec; ++i) body[n++] = '0';
    }
    useE = 0;
  }
  if (useE) {
    body[n++] = upper ? 'E' : 'e';
    int ex = a == 0 ? 0 : exp10;
    body[n++] = ex < 0 ? '-' : '+';
    if (ex < 0) ex = -ex;
    if (ex >= 100) body[n++] = (char)('0' + ex / 100);
    body[n++] = (char)('0' + (ex / 10) % 10);
    body[n++] = (char)('0' + ex % 10);
  }
  (void)cap;
  return (int)n;
}

int OTE_VSNPRINTF(char* out, size_t cap, const char* fmt, va_list ap) {
  size_t len = 0;
  for (const char* p = fmt; *p;) {
    if (*p != '%') {
      const char* q = p;
      while (*q && *q != '%') ++q;
      emit(out, cap, &len, p, (size_t)(q - p));
      p = q;
      continue;
    }
    ++p;
    int flags = 0, width = 0, prec = -1;
    for (;; ++p) {
      if (*p == '-') flags |= kFlagMinus;
      else if (*p == '+') flags |= kFlagPlus;
      else if (*p == ' ') flags |= kFlagSpace;
      else if (*p == '#') flags |= kFlagHash;
      else if (*p == '0') flags |= kFlagZero;
      else break;
    }
    if (*p == '*') {
      width = va_arg(ap, int);
      if (width < 0) { flags |= kFlagMinus; width = -width; }
      ++p;
    } else {
      while (*p >= '0' && *p <= '9') width = width * 10 + (*p++ - '0');
    }
    if (*p == '.') {
      ++p;
      prec = 0;
      if (*p == '*') {
        prec = va_arg(ap, int);
        ++p;
      } else {
        while (*p >= '0' && *p <= '9') prec = prec * 10 + (*p++ - '0');
      }
    }
    int lng = 0;  // 0 int, 1 long, 2 long long, 3 long double, -1 short, -2 char
    for (;; ++p) {
      if (*p == 'l') lng = lng == 1 ? 2 : 1;
      else if (*p == 'h') lng = lng == -1 ? -2 : -1;
      else if (*p == 'L') lng = 3;
      else if (*p == 'z' || *p == 't') lng = 1;
      else if (*p == 'j') lng = 2;
      else break;
    }
    const char conv = *p ? *p++ : 0;
    switch (conv) {
      case '%':
        emit(out, cap, &len, "%", 1);
        break;
      case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': {
        long double v = lng == 3 ? va_arg(ap, long double) : (long double)va_arg(ap, double);
        char sign = 0;
        if (v < 0 || (v == 0 && 1.0L / v < 0)) { sign = '-'; v = -v; }
        else if (flags & kFlagPlus) sign = '+';
        else if (flags & kFlagSpace) sign = ' ';
        char body[400];
        int bl = floatBody(body, sizeof body, v, conv, prec, flags & kFlagHash);
        int total = bl + (sign ? 1 : 0);
        const int finite = v == v && v < 1e4000L;
        if (!(flags & kFlagMinus) && !((flags & kFlagZero) && finite)) emitRepeat(out, cap, &len, ' ', width - total);
        if (sign) emit(out, cap, &len, &sign, 1);
        if (!(flags & kFlagMinus) && (flags & kFlagZero) && finite) emitRepeat(out, cap, &len, '0', width - total);
        emit(out, cap, &len, body, (size_t)bl);
        if (flags & kFlagMinus) emitRepeat(out, cap, &len, ' ', width - total);
        break;
      }
      case 's': {
        const char* s = va_arg(ap, const char*);
        if (!s) s = "(null)";
        size_t sl = 0;
        while (s[sl] && (prec < 0 || (int)sl < prec)) ++sl;
        if (!(flags & kFlagMinus)) emitRepeat(out, cap, &len, ' ', width - (int)sl);
        emit(out, cap, &len, s, sl);
        if (flags & kFlagMinus) emitRepeat(out, cap, &len, ' ', width - (int)sl);
        break;
      }
      case 'c': {
        char c = (char)va_arg(ap, int);
        if (!(flags & kFlagMinus)) emitRepeat(out, cap, &len, ' ', width - 1);
        emit(out, cap, &len, &c, 1);
        if (flags & kFlagMinus) emitRepeat(out, cap, &len, ' ', width - 1);
        break;
      }
      case 'd': case 'i': case 'u': case 'x': case 'X': case 'o': case 'p': {
        // Rebuild the specification and let the C library format this one integer / pointer.
        char spec[40];
        size_t sp = 0;
        spec[sp++] = '%';
        if (flags & kFlagMinus) spec[sp++] = '-';
        if (flags & kFlagPlus) spec[sp++] = '+';
        if (flags & kFlagSpace) spec[sp++] = ' ';
        if (flags & kFlagHash) spec[sp++] = '#';
        if (flags & kFlagZero) spec[sp++] = '0';
        sp += (size_t)snprintf(spec + sp, sizeof spec - sp, "%d", width);
        if (prec >= 0) sp += (size_t)snprintf(spec + sp, sizeof spec - sp, ".%d", prec);
        char tmp[400];
        int tl;
        if (conv == 'p') {
          spec[sp++] = 'p';
          spec[sp] = 0;
          tl = snprintf(tmp, sizeof tmp, spec, va_arg(ap, void*));
        } else if (lng == 2) {
          spec[sp++] = 'l'; spec[sp++] = 'l'; spec[sp++] = conv; spec[sp] = 0;
          if (conv == 'd' || conv == 'i') tl = snprintf(tmp, sizeof tmp, spec, va_arg(ap, long long));
          else tl = snprintf(tmp, sizeof tmp, spec, va_arg(ap, unsigned long long));
        } else if (lng == 1) {
          spec[sp++] = 'l'; spec[sp++] = conv; spec[sp] = 0;
          if (conv == 'd' || conv == 'i') tl = snprintf(tmp, sizeof tmp, spec, va_arg(ap, long));
          else tl = snprintf(tmp, sizeof tmp, spec, va_arg(ap, unsigned long));
        } else {
          spec[sp++] = conv; spec[sp] = 0;  // short and char arrive promoted to int
          if (conv == 'd' || conv == 'i') {
            int v = va_arg(ap, int);
            if (lng == -1) v = (short)v;
            else if (lng == -2) v = (signed char)v;
            tl = snprintf(tmp, sizeof tmp, spec, v);
          } else {
            unsigned v = va_arg(ap, unsigned);
            if (lng == -1) v = (unsigned short)v;
            else if (lng == -2) v = (unsigned char)v;
            tl = snprintf(tmp, sizeof tmp, spec, v);
          }
        }
        if (tl > (int)sizeof tmp - 1) tl = (int)sizeof tmp - 1;
        if (tl > 0) emit(out, cap, &len, tmp, (size_t)tl);
        break;
      }
      case 'n':
        (void)va_arg(ap, int*);
        break;
      default:  // unknown conversion: print it as it was written
        emit(out, cap, &len, "%", 1);
        if (conv) emit(out, cap, &len, &conv, 1);
        break;
    }
  }
  if (cap > 0) out[len < cap ? len : cap - 1] = 0;
  return (int)len;
}

int OTE_SNPRINTF(char* out, size_t cap, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = OTE_VSNPRINTF(out, cap, fmt, ap);
  va_end(ap);
  return n;
}
