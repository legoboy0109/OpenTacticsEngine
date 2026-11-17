// The difficulty in saves (docs/DIFFICULTY.md, docs/SAVES.md): a battle save carries the id of the difficulty the battle was
// built with, a campaign save's "playthrough" the one the playthrough is on. Both only in a game that has difficulties, so
// every other game saves byte for byte as before. A saved id the game no longer has falls back to the game's default
// (logged); a damaged value (not text) is refused.
#pragma once
#include "core/battle/battle.h"
#include "core/battle/save_reader.h"

namespace ote {

struct Playthrough;  // core/campaign/playthrough.h

namespace savejson {

void writeBattleDifficulty(const Battle& b, Json& out);
bool readBattleDifficulty(Reader& rd, const Json& saved, Battle& b);
void writePlaythroughDifficulty(const Database& db, const Playthrough& pt, Json& out);
bool readPlaythroughDifficulty(Reader& rd, const Database& db, const Json& saved, Playthrough& pt);

}  // namespace savejson
}  // namespace ote
