#include "platform/common/bitmap_font.h"

extern "C" char font8x8_basic[128][8];  // bitmap_font_data.c

namespace ote {

const unsigned char* bitmapGlyph(char c) {
  unsigned char index = (unsigned char)c;
  if (index < 32 || index > 126) index = 32;
  return (const unsigned char*)font8x8_basic[index];
}

}  // namespace ote
