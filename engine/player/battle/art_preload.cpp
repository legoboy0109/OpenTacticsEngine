#include "battle/art_preload.h"

#include <vector>

namespace ote {

namespace {

void askFor(const Sprites& sp, const Look& look) {
  if (!look.any()) return;
  sp.request(look.sheet);
  if (look.walk.any()) {
    bool turned = false;  // a walk direction drawn as the opposite one needs the mirrored copy
    for (int d = 0; d < kWalkDirs; ++d) turned |= look.walk.mirrored[d];
    sp.request(look.walk.sheet, turned);
  }
}

void askFor(const Sprites& sp, const UnitLook& ul, const std::string& sideId) {
  askFor(sp, ul.forSide(sideId));
}

}  // namespace

void preloadMapArt(const Sprites& sp, const Battle& b, int styleIndex) {
  if (!sp.any() || !b.db) return;
  const Graphics& g = b.db->graphics;
  const GraphicsSet* set = g.set(styleIndex);
  if (!set) return;
  for (const Unit& u : b.units) {
    if (u.type < 0 || (size_t)u.type >= set->units.size() || u.side < 0 || (size_t)u.side >= b.sides.size()) continue;
    askFor(sp, set->units[(size_t)u.type], b.sides[(size_t)u.side].id);
  }
  std::vector<char> seen(set->terrain.size(), 0);  // each terrain kind once, not every square
  for (const CellState& c : b.cells) {
    if (c.terrain < 0 || (size_t)c.terrain >= seen.size() || seen[(size_t)c.terrain]) continue;
    seen[(size_t)c.terrain] = 1;
    askFor(sp, set->terrain[(size_t)c.terrain]);
  }
  for (const StructureState& s : b.structures) {
    if (s.type < 0 || (size_t)s.type >= set->structures.size()) continue;
    const std::string none;
    askFor(sp, set->structures[(size_t)s.type],
           s.owner >= 0 && (size_t)s.owner < b.sides.size() ? b.sides[(size_t)s.owner].id : none);
  }
  if (b.hasTiles() && b.tileset >= 0 && (size_t)b.tileset < g.tilesets.size()) sp.request(g.tilesets[(size_t)b.tileset].sheet);
}

}  // namespace ote
