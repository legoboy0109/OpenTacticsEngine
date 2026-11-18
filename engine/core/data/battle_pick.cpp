#include "core/data/battle_pick.h"

namespace ote {

namespace {

int entry(const std::vector<int>& table, int terrain) {
  return terrain >= 0 && (size_t)terrain < table.size() ? table[(size_t)terrain] : -1;
}

}  // namespace

int pickBattleBackground(const Database& db, const MapDef& map, int terrain) {
  const BattleArt& art = db.graphics.battle;
  int bg = entry(map.bgByTerrain, terrain);
  if (bg < 0) bg = map.battleBackground;
  if (bg < 0) bg = map.bgDefault;
  if (bg < 0) bg = entry(art.terrainBackground, terrain);
  if (bg < 0) bg = art.defaultBackground;
  return bg;
}

int pickBattlePlatform(const Database& db, const MapDef& map, int terrain) {
  const BattleArt& art = db.graphics.battle;
  int p = entry(map.platformByTerrain, terrain);
  if (p < 0) p = map.platformDefault;
  if (p < 0) p = entry(art.terrainPlatform, terrain);
  if (p < 0) p = art.defaultPlatform;
  return p;
}

}  // namespace ote
