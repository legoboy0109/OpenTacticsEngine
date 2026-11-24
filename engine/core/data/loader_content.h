// Loaders for the data/*.json files. Call order matters because of cross
// references: weapons -> items -> unit types -> terrain -> structures -> checkVersusKeys.
#pragma once
#include "core/data/database.h"

namespace ote {

bool loadWeapons(const std::string& path, Database& db, std::string& err);
// Optional data/items.json (docs/ITEMS.md); needs the rules, stats, resources and weapons.
bool loadItems(const std::string& path, Database& db, std::string& err);
// Optional data/research.json (docs/ECONOMY.md "Research"); needs stats, resources and unit types.
bool loadResearch(const std::string& path, Database& db, std::string& err);
bool loadUnitTypes(const std::string& path, Database& db, std::string& err);
bool loadTerrain(const std::string& path, Database& db, std::string& err);
// Optional data/structures.json (docs/STRUCTURES.md); needs move types, unit
// types and terrain loaded. A missing file means the game has no structures.
bool loadStructures(const std::string& path, Database& db, std::string& err);
// Weapons load before unit types, so "versus" keys are checked afterwards.
bool checkVersusKeys(const std::string& weaponsPath, const Database& db, std::string& err);
// Optional data/graphics.json; needs terrain, unit types and styles loaded.
// A missing file is not an error (the game then has no art).
bool loadGraphics(const std::string& path, Database& db, std::string& err);

class LoadCtx;
class Json;
// Repair and supply (loader_supply.cpp; docs/ECONOMY.md): the pieces of the data files, each optional.
bool loadOptionRules(const LoadCtx& c, const Json& rules, Database& db);  // "rules.research" and "rules.fog" (loader_options.cpp)
// The shared reading of { "rosterAlive": id, "party": "any" | "active" } (also rosterDead, inRoster), for map events and campaign scenes (loader_story_roster.cpp).
bool readRosterCondition(const LoadCtx& c, const Json& e, const std::string& where, const std::string& kind, int& mode, bool& anyParty,
                         std::string& id);
bool loadProgressionExtras(const LoadCtx& c, const Json& block, Database& db);  // "xpNoDamage", "computerGains" (loader_fidelity.cpp)
bool readAbilityExtras(const LoadCtx& c, Database& db, const Json& e, const std::string& where, AbilityDef& a);  // an ability's "xp", "inflict"
bool loadSupplyRules(const LoadCtx& c, const Json& rules, Database& db);  // game.json "rules.supply"
// game.json "rules.items.freeBattleStorage" (loader_free_stock.cpp): after the items and resources are known.
bool loadFreeBattleStock(const std::string& gameFile, Database& db, std::string& err);
bool loadAiRules(const std::string& gameFile, Database& db, std::string& err);  // game.json "rules.ai" (loader_ai_rules.cpp; docs/AI.md)
// One set of attack weights, as "rules.ai.weights" writes them (also a faction's "ai"."weights"; loader_ai_rules.cpp). `where` names it in
// messages, `name` goes into the set; keys left out keep the AiWeightSet defaults.
bool readAiWeightSet(const LoadCtx& c, const Database& db, const std::string& where, const std::string& name, const Json& e, AiWeightSet& out);
// "fire" and "area" of a weapon (loader_weapon_area.cpp; docs/SRPG.md "Area attacks and line fire").
bool readWeaponArea(const LoadCtx& c, const Json& e, const std::string& where, WeaponDef& w);
// An area object's "shape", "radius", "length" (loader_weapon_area.cpp); weapons and abilities both use it. Other keys are the caller's.
bool readAreaShape(const LoadCtx& c, const Json& a, const std::string& where, AreaDef& d);
bool readWeaponAmmo(const LoadCtx& c, const Json& e, const std::string& where, WeaponDef& w);
bool readUnitSupply(const LoadCtx& c, const Json& e, const std::string& where, UnitTypeDef& u);
bool readStructureRepairs(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, StructureDef& s);
// After items: "ammo" on a weapon that is carried as an item is an error.
bool checkAmmoNotItems(const std::string& weaponsPath, const Database& db, std::string& err);
// Fog of war (loader_fog.cpp; docs/ECONOMY.md): units.json "vision", terrain / structures "visionBonus" and
// "hides", the map's "fog" (after the sides are loaded).
bool readUnitVision(const LoadCtx& c, const Json& e, const std::string& where, UnitTypeDef& u);
// "visionBonus" is a plain number (every unit) or {"amount": 0..10, "units": [type ids or tags]} (those units only);
// `visionUnits` stays empty for the plain form. Needs the unit types loaded (the keys are checked against them).
bool readSquareVision(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, int& visionBonus,
                      std::vector<std::string>& visionUnits, bool& hides);
// A unit type id, or a tag some unit type carries (the rule of "versus" and "repairs" keys).
bool isUnitKey(const Database& db, const std::string& key);
// game.json "grid" (loader_grid.cpp; docs/HEX.md): sets db.gridType and db.hexLayout. Missing = square.
bool readGameGrid(const LoadCtx& c, const Json& root, Database& db);
bool readMapFog(const LoadCtx& c, const Json& root, MapDef& m);
// structures.json "light": 1..10 (a fixed light; loader_fog.cpp), and the map side's "aiSeesAll" (read by loadSides).
bool readStructureLight(const LoadCtx& c, const Json& e, const std::string& where, StructureDef& s);
// An item's "light": { "radius", "turns", "shrink" } (loader_fog.cpp): a usable item that lights the square its user stands on.
bool readItemLight(const LoadCtx& c, const Json& e, const std::string& where, ItemDef& it);
// The "changes" of a map (loader_map_changes.cpp; docs/EVENTS.md "Changing the map"); after the map's structures are loaded.
bool readMapChanges(const LoadCtx& c, const Database& db, const Json& root, MapDef& m);
// The "upgrades" of every structure kind, read once all kinds exist (loader_upgrades.cpp). `list` is the
// "structures" array of data/structures.json; db.structures already holds its entries in the same order.
bool loadStructureUpgrades(const LoadCtx& c, const Json& list, Database& db);
// The side index of a map side id, -1 if there is none (map_loader.cpp).
int findMapSide(const MapDef& m, const std::string& id);
// A unit entry of a map or of the addUnit command, in two halves (map_loader.cpp): what it is and
// where (checks type, side, position inside the map; `where` is extended to name the unit),
// then its stats and items.
bool readUnitHead(const LoadCtx& c, const Database& db, const MapDef& m, const Json& e, std::string& where,
                  MapUnitDef& u);
bool readUnitBody(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, MapUnitDef& u);
// "behaviour" of a map unit / addUnit (loader_behaviour.cpp; docs/AI.md). checkBehaviourPlace: the squares and the place exist on this map.
// checkBehaviourUnits: a unit it heads for / prefers is one of `ids` or a character. checkMapBehaviours: the latter for every map unit.
bool readUnitBehaviour(const LoadCtx& c, const Database& db, const MapDef& m, const Json& e, const std::string& where, MapUnitDef& u);
bool checkBehaviourPlace(const LoadCtx& c, const MapDef& m, const std::string& where, const AiBehaviour& b);
bool checkBehaviourUnits(const LoadCtx& c, const Database& db, const std::vector<std::string>& ids, const std::string& where,
                         const AiBehaviour& b);
bool checkMapBehaviours(const Database& db, const Json& root, const MapDef& m, const std::string& file, std::string& err);
// Story ids of the units that addUnit commands create (loader_events.cpp).
void collectAddedUnitIds(const Json& commands, std::vector<std::string>& out);
// The ids the addUnit commands of a showMap section create: the event's own list from entry `from` up to its returnMap (loader_events.cpp).
void collectShownUnitIds(const Json& commands, size_t from, std::vector<std::string>& out);
// Names of guard groups in addUnit / setBehaviour blocks of a command list (loader_behaviour.cpp).
void collectBehaviourGroups(const Json& commands, std::vector<std::string>& out);
// A list of items handed out by data ("items" of a unit type or a map unit, "storage" of a map side):
// ids or { "item", "uses", "drop" }. A missing key gives an empty list. maxCount < 0 = any length.
bool readItemGrants(const LoadCtx& c, const Database& db, const Json& o, const char* key, const std::string& where,
                    bool allowDrop, int maxCount, std::vector<ItemGrant>& out);

}  // namespace ote
