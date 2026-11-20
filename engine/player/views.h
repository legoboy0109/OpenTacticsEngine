// Which kinds of view (docs/VIEWS.md) this build of the player can draw. A
// style of a kind it cannot draw is not offered and never chosen; the 3D
// kinds join the list when the platform has a 3D renderer.
#pragma once
#include "core/data/database.h"

namespace ote {

inline bool viewSupported(StyleView v) {
#ifdef OTE_NXDK
  // Original Xbox: the 3D kinds need the software 3D drawer, about 300 ms a frame on its processor (docs/XBOX_OG.md).
  // A style of such a kind is neither offered nor chosen; a game whose first style is 3D opens in its first flat one.
  return v == StyleView::TopDown || v == StyleView::Isometric;
#else
  return v == StyleView::TopDown || v == StyleView::Isometric || v == StyleView::Isometric3D || v == StyleView::Full3D;
#endif
}

}  // namespace ote
