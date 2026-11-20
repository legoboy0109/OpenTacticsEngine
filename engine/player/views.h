// Which kinds of view (docs/VIEWS.md) this build of the player can draw. A
// style of a kind it cannot draw is not offered and never chosen; the 3D
// kinds join the list when the platform has a 3D renderer.
#pragma once
#include "core/data/database.h"

namespace ote {

inline bool viewSupported(StyleView v) {
  // (The original Xbox draws the 3D kinds on its NV2A chip, docs/XBOX_OG.md "3D"; software 3D would take 300 ms a frame there.)
  return v == StyleView::TopDown || v == StyleView::Isometric || v == StyleView::Isometric3D || v == StyleView::Full3D;
}

}  // namespace ote
