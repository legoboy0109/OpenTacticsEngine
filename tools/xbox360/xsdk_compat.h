/* Force-included into every source of the Xbox 360 build (CMakeLists.txt, `if(XSDK)`).
 *
 * xsdk's GCC 15 + newlib give C++ in strict mode (-std=c++17) no LLONG_MAX: GCC's limits.h defines it only for C99 or when
 * glibc's limits.h does, and newlib's does not. Lua (luaconf.h) takes its absence for a compiler without long long. */
#include <limits.h>
#ifndef LLONG_MAX
#define LLONG_MAX __LONG_LONG_MAX__
#define LLONG_MIN (-LLONG_MAX - 1LL)
#define ULLONG_MAX (LLONG_MAX * 2ULL + 1ULL)
#endif
