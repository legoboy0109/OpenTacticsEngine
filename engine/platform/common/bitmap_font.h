// Built-in 8x8 ASCII font shared by every platform backend, so text looks
// identical everywhere and backends need no font support of their own
// (SDL2, console SDKs...). Data: third_party/font8x8 (public domain).
#pragma once

namespace ote {

// 8 bytes, one per pixel row from the top; bit 0 is the LEFTMOST pixel.
// Characters outside printable ASCII return a blank glyph.
const unsigned char* bitmapGlyph(char c);

}  // namespace ote
