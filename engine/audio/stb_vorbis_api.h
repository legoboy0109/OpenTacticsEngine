// stb_vorbis's declarations for C++ (the implementation is stb_vorbis_impl.c).
// The defines must match the ones in stb_vorbis_impl.c.
#pragma once
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_HEADER_ONLY
extern "C" {
#include "../../third_party/stb/stb_vorbis.c"
}
#undef STB_VORBIS_HEADER_ONLY
