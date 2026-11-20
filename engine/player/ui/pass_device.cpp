#include "ui/pass_device.h"

#include <algorithm>
#include <vector>

#include "battle/fog_text.h"
#include "ui/input_hints.h"
#include "ui/text.h"

namespace ote {

void PassDevice::open(const std::string& side, Color sideColor) {
  open_ = true;
  side_ = side;
  color_ = sideColor;
}

bool PassDevice::confirmed(const InputEvent& e) const {
  return open_ && (e.type == InputEvent::PointerDown || (e.type == InputEvent::ButtonDown && e.button == Button::Confirm));
}

void PassDevice::draw(Renderer& r) const {
  if (!open_) return;
  r.fillRect(0, 0, r.width(), r.height(), Color(0, 0, 0));
  const int maxW = std::max(40, r.width() - 16);
  const std::string text = std::string(FogWords::kPassTo) + side_;
  const int sc = fitScale(r, text, maxW, 2);
  std::vector<std::string> lines = wrapText(r, text, maxW, sc);
  if (lines.size() > 3) lines.resize(3);
  const int lh = r.lineHeight(sc);
  const int h = (int)lines.size() * lh + 8 + r.lineHeight();
  int y = (r.height() - h) / 2;
  r.fillRect(0, y - 6, r.width(), 4, color_);  // a stripe in the side's colour, so it is clear who is next
  for (const std::string& ln : lines) {
    drawTextCentered(r, r.width() / 2, y, ln, Color(240, 242, 255), sc);
    y += lh;
  }
  drawTextCentered(r, r.width() / 2, y + 8, hintText(FogWords::kPassHint), Color(160, 166, 195));
}

}  // namespace ote
