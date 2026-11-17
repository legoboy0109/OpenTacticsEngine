// Loader side of doors, chests and visitable places (docs/SRPG.md section 10).
#pragma once
#include "core/data/database.h"

namespace ote {

class LoadCtx;
class Json;

// game.json "rules.places": { "actions": "menu" | "auto" } (optional).
bool loadPlaceRules(const LoadCtx& c, const Json& rules, Database& db);
// An item's "opens" (data/items.json): ids are checked later, once structures exist (loadStructurePlaces).
bool readItemOpens(const LoadCtx& c, const Json& e, const std::string& where, ItemDef& it);
// The "door" / "chest" / "visit" blocks and "blocksSight" of every structure kind, read once all kinds exist. `list` is the
// "structures" array of data/structures.json; db.structures holds its entries in the same order.
bool loadStructurePlaces(const LoadCtx& c, const Json& list, Database& db);
// One placed structure's own keys in a map file: "id", "contents", "locked", "visitSides", "visitUnits" (map_loader.cpp calls
// this once the structure's kind is known and its footprint checked).
bool readPlacedPlace(const LoadCtx& c, const Database& db, const MapDef& m, const Json& e, const std::string& where,
                     const std::vector<Cell>& cells, MapStructureDef& s);
// After the map's units are loaded: every "visitUnits" id names one of them (or a character).
bool checkPlaceVisitors(const LoadCtx& c, const Database& db, const MapDef& m);
// "goesFor" of a map unit / addUnit: ["chests", "villages", "doors"].
bool readUnitGoals(const LoadCtx& c, const MapDef& m, const Json& e, const std::string& where, MapUnitDef& u);
// The ability effect "unlock" and the staff effect "unlock" share this reading: true | ["door", "chest"] | (ability only)
// { "kinds": [...], "needs": "item id" }. `needs` is the item id as written ("" = none).
bool readUnlockKinds(const LoadCtx& c, const Json& v, const std::string& where, bool allowNeeds, bool& doors, bool& chests,
                     std::string& needs);
// The key rule: does this item open a structure kind? The item's "opens" names "door" / "chest" for a door / chest kind, or the
// kind's id; and, when the kind lists "keys", the item is one of them.
bool kindAcceptsKey(const Database& db, const StructureDef& kind, int kindIndex, int item);
bool itemOpensKind(const Database& db, int item, int structureKind);

}  // namespace ote
