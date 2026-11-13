#include "ui/chrome.h"

#include "ui/text.h"

namespace ote {

static int titleScale(const Renderer& r) { return r.lineHeight() <= 11 ? 2 : 1; }

int drawTitle(Renderer& r, const std::string& title) {
  r.clear(chrome::kBg);
  int w = r.width();
  int sc = fitScale(r, title, w - 16, titleScale(r));
  drawTextCentered(r, w / 2, 8, fitText(r, title, w - 16, sc), chrome::kTitle, sc);
  return 8 + r.lineHeight(sc) + 8;
}

int footerHeight(const Renderer& r) { return r.lineHeight() + 6; }

int drawFooter(Renderer& r, const std::string& text) {
  int y = r.height() - r.lineHeight() - 3;
  drawTextCentered(r, r.width() / 2, y, fitText(r, text, r.width() - 12), chrome::kHint);
  return y;
}

void drawRowBox(Renderer& r, int x, int y, int w, int h, bool selected) {
  r.fillRect(x, y, w, h, selected ? chrome::kRowSel : chrome::kRow);
  if (selected) r.drawRect(x, y, w, h, chrome::kAccent);
}

}  // namespace ote
