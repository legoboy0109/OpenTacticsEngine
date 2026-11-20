// Force-included (-include) into every nxdk build unit. nxdk's C library ships stubs that assert(0) for strtod and a few
// math functions the engine, Lua and stb use. These renames send those calls to nx_compat.c instead; the stub objects
// stay unused, so there is no duplicate-symbol clash. Add a name here and in nx_compat.c when a new "Assertion failed:
// '0' in function 'xxx'" shows up on the screen.
#pragma once
#define strtod ote_strtod
#define lround ote_lround
#define llround ote_llround
#define lroundf ote_lroundf
#define llroundf ote_llroundf
#define lrint ote_lrint
#define llrint ote_llrint
#define lrintf ote_lrintf
#define llrintf ote_llrintf
#define log2 ote_log2
#define exp2 ote_exp2
#define exp2f ote_exp2f
#define cbrt ote_cbrt
#define log1p ote_log1p
#define expm1 ote_expm1
#define scalbn ote_scalbn
#define fdim ote_fdim
// nxdk's getenv finds nothing and there is no putenv: the args file's NAME=value words (OTE_LOG_LEVEL ...) live in a small
// table of ours (nx_compat.c, ote_setenv).
#define getenv ote_getenv
// nxdk's printf has no floating-point conversions (%f %e %g print nothing): nx_printf.c replaces snprintf / vsnprintf.
#define snprintf ote_snprintf
#define vsnprintf ote_vsnprintf
