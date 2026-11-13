// The ONE translation unit that compiles stb_image's implementation
// (third_party/stb, public domain). PNG only, no stdio (callers read files).
// Third-party warnings are silenced for this file only.
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#include "../../../third_party/stb/stb_image.h"
