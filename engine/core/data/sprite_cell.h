// One cell of a sheet, [column, row] counted from 0 (docs/GRAPHICS.md).
#pragma once
#include <cstdint>

namespace ote {

struct SpriteCell {
  int16_t col = 0, row = 0;
  bool operator==(const SpriteCell& o) const { return col == o.col && row == o.row; }
};

}  // namespace ote
