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

// A style the player may offer: a kind of view it can draw, and art that fits the machine. The original Xbox has 64 MB
// in all; a style marked "largeArt" (docs/GAME_FORMAT.md "styles") does not fit there (docs/XBOX_OG.md "Limits").
template <class Style>
inline bool styleUsable(const Style& s) {
#ifdef OTE_NXDK
  if (s.largeArt) return false;
#endif
  return viewSupported(s.view);
}

}  // namespace ote
