// Shared look of the launcher screens (home, game, options, lists): dark
// background, title at the top, one hint line at the bottom. Everything is
// measured from the active font; nothing assumes a glyph size.
#pragma once
#include <string>

#include "platform/platform.h"

namespace ote {

namespace chrome {
const Color kBg(18, 20, 32);
const Color kAccent(255, 220, 90);
const Color kTitle(235, 238, 255);
const Color kText(215, 218, 235);
const Color kTextSel(255, 255, 255);
const Color kDim(150, 156, 185);
const Color kHint(120, 128, 160);
const Color kRow(30, 34, 54);
const Color kRowSel(60, 78, 130);
const Color kPanel(24, 28, 46);
}  // namespace chrome

// Clears to the background and draws the centred title (doubled when the font
// is small enough to afford it). Returns the y just below the title block.
int drawTitle(Renderer& r, const std::string& title);
// Hint line at the very bottom (truncated to fit). Returns its top y.
int drawFooter(Renderer& r, const std::string& text);
// Height reserved by the footer, for layout.
int footerHeight(const Renderer& r);
// A selectable row background (with accent outline when selected).
void drawRowBox(Renderer& r, int x, int y, int w, int h, bool selected);

}  // namespace ote
