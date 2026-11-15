/* The ONE translation unit that compiles stb_vorbis (third_party/stb, public
 * domain / MIT), as C, with warnings off for this file only (see
 * the top-level CMakeLists.txt).
 * No stdio (callers read files) and no push-data API (we pull from memory).
 * vorbis.cpp sees the declarations through stb_vorbis_api.h, which sets the
 * same defines. */
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "../../third_party/stb/stb_vorbis.c"
