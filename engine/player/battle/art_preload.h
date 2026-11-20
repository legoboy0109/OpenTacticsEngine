// Asking for the art a scene is about to draw, up front, so the PNGs decode while the scene opens and not in the middle
// of an animation (docs/GRAPHICS.md "Art memory"). Only a head start: whatever is not asked for here loads when first drawn.
#pragma once
#include "battle/sprites.h"
#include "core/battle/battle.h"

namespace ote {

// The sheets a battle map shows at its start: its units (standing and walking pictures), its terrain, the structures on it
// and the tileset of a hand-painted map, in the visual style `styleIndex`.
void preloadMapArt(const Sprites& sprites, const Battle& battle, int styleIndex);

}  // namespace ote
