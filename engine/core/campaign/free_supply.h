// The stocked supply of a FREE battle (game.json rules.items.freeBattleStorage; docs/GAME_FORMAT.md "Free battles"). Only the
// player's free-battle entry points call this; a campaign chapter (also a `--campaign --chapter` test playthrough) never does.
#pragma once
#include <vector>

#include "core/data/database.h"

namespace ote {

// What the storage holds at the start: for every item in the game's order, `each` copies (full uses) unless excepted, with
// the counts of "items" overriding. Empty for a game without the key or without a shared storage (rules.items.storage "off").
// The rules put no size limit on the storage, so none applies here.
std::vector<ItemGrant> freeBattleSupply(const Database& db);

// Adds the supply to the storage of every human side of `map`, and "funds" to those sides' resources. Does nothing for a
// game without the key.
void applyFreeSupply(const Database& db, MapDef& map);

}  // namespace ote
