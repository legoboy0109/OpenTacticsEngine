// The research and structure-kind parts of a battle save (docs/SAVES.md), used by save.cpp and, for the
// list of finished technologies, by the campaign save. Every key is optional and only written when it says
// something, so saves of games without upgrades or research stay byte for byte what they were.
#pragma once
#include "core/base/json.h"
#include "core/battle/battle.h"
#include "core/battle/save_reader.h"

namespace ote {
namespace savejson {

// A side's "research": { "done": [technology indices, ascending], "current": index, "left": turns };
// "current" and "left" only while one is under way. Null (a Json with no value) when there is nothing to say.
bool researchJson(const SideState& s, Json& out);
// Reads it into `s` (whose `researched` is already sized); a missing key means "nothing finished, nothing
// under way". Checks ranges, order, that every finished or running technology has its prerequisites
// finished, and that `left` fits the technology's turns.
bool readSideResearch(Reader& rd, const Database& db, const Json& side, const std::string& where, SideState& s);

// A plain list of technology indices (ascending, no repeats), as the campaign carries them.
Json techListJson(const std::vector<int>& techs);
bool readTechList(Reader& rd, const Database& db, const Json& o, const char* key, const std::string& where,
                  std::vector<int>& out);

// A structure's saved kind: "type" is written only when it differs from the map's, and read back with a
// check that the kind has the map kind's footprint and can be reached from it through upgrades.
void writeStructureKind(const StructureState& s, Json& out);
// The checks alone: kind `type` has the footprint of `mapType` and is reachable from it through upgrades.
// `extra` (optional): kinds that count as reachable too (the kinds the map's changes turn this structure into).
bool checkStructureKind(Reader& rd, const Database& db, int type, int mapType, const std::string& at, const std::vector<int>* extra = nullptr);
bool readStructureKind(Reader& rd, const Database& db, const Json& j, const std::string& where, StructureState& st, const std::vector<int>* extra = nullptr);

}  // namespace savejson
}  // namespace ote
