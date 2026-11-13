// The ONE translation unit that compiles stb_truetype's implementation
// (third_party/stb, public domain). Third-party warnings are silenced for
// this file only; the project's own files keep -Wall -Wextra.
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../../third_party/stb/stb_truetype.h"
