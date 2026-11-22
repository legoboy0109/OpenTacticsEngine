// How a computer-controlled unit behaves (docs/AI.md): its movement stance, when it starts, its group and an optional
// favourite target. Pure data, shared by a map unit, an `addUnit` command, the `setBehaviour` story command and the live
// Unit (which also saves it). The default value is exactly the computer opponent's behaviour before this existed.
#pragma once
#include <string>
#include <utility>
#include <vector>

#include "core/base/json.h"

namespace ote {

struct Database;

enum class AiMode {
  Advance,    // default: walks toward the enemy and attacks
  Hold,       // never moves; attacks and uses staves on what it can reach from its square
  Guard,      // waits until a hostile unit is within its threat range, then acts like Advance (or stays reactive)
  GuardArea,  // waits until a hostile unit is inside an area or within N squares, then acts like Advance
  Seek,       // heads for a named unit, a structure (place) or a square
  Patrol,     // walks a route of squares, attacking whatever it can reach on the way (docs/AI.md "Patrol route")
  Loot,       // goes for the chests, villages and doors it is marked for (and steals) before anything else; then escapes (docs/AI.md "Looting and escaping")
  Escape      // walks to the nearest of the map's "exits" and leaves the map there; never attacks
};

// A soft preference between targets (a bonus on top of the usual scoring, never a veto).
enum class AiPrefer { None, Weakest, Nearest, Unit, Type, Tag };

// "retreat": a hurt unit falls back and heals (docs/AI.md "Retreat to heal"). The state (whether it is retreating now) lives on the Unit.
struct AiRetreat {
  bool on = false;
  int below = 0;               // enters retreat when its hit points are under this percent at the start of its turn
  int until = 0;               // leaves it when they are back at this percent or more
  bool useItems = true;        // drinks a healing item it carries before walking
  std::vector<std::string> terrainIds;  // "to": { "terrain": [...] } as written (empty = just away from the enemy)
  std::vector<int> terrains;            // the same as indices into Database::terrains
};

struct AiBehaviour {
  AiMode mode = AiMode::Advance;
  bool react = false;     // Guard: "wake": "react" (never wakes for good: attacks what it can reach, never approaches)
  int fromTurn = 0;       // does nothing in rounds before this one; 0 = from the first round
  std::string group;      // Guard / GuardArea: when one member wakes, all wake ("" = none)
  // GuardArea: wakes when a hostile unit stands inside the rectangle and / or within `within` squares of the unit.
  bool hasArea = false;
  int ax = 0, ay = 0, aw = 1, ah = 1;
  int within = 0;         // 0 = not used
  // Seek: exactly one target.
  std::string seekUnit;   // a unit's story id
  std::string seekPlace;  // a placed structure's id
  bool seekCell = false;  // a square
  int sx = 0, sy = 0;
  // Target preference (any mode).
  AiPrefer prefer = AiPrefer::None;
  std::string preferText;  // Unit: the unit id; Type: the unit type id; Tag: the tag
  int preferType = -1;     // Type: index into Database::unitTypes
  // Scripted-style computer players (docs/AI.md "Scripted-style computer players"); every key opt-in.
  int order = 0;                 // "order": the place in the side's acting order when rules.ai.turnOrder is "order"
  std::string weightsName;       // "weights": the name of a set of rules.ai.weights ("" = the default attack scoring)
  int weightSet = -1;            // the same as an index into Database::rules.ai.weights
  bool cautionNone = false;      // "caution": "none"
  AiRetreat retreat;             // "retreat"
  int idleTurns = 0;             // "idleTurns": its first n own turns on the map it makes no approach move (0 = off)
  std::vector<std::pair<int, int>> route;  // Patrol: the squares (x, y) it walks, in order, looping
  std::vector<std::string> ignores;        // "ignores": unit / character ids it never chases or attacks (unless nothing else is left)
  bool areaMoveCost = false;     // GuardArea "measure": "moveCost": `within` is a movement cost, not a distance
  bool healsRetreating = false;  // "heals": "retreating": its healing staves are used only on retreating allies (docs/AI.md "Healers that serve retreating allies")
  bool noAttack = false;         // "attacks": false: it never starts a fight (counterattacks are rules and still happen); any mode

  bool isDefault() const;
};

constexpr int kMaxBehaviourTurn = 99;    // "fromTurn" 1..99
constexpr int kMaxBehaviourWithin = 99;  // "within" 1..99
constexpr int kMaxBehaviourOrder = 100000;  // "order" -100000..100000
constexpr int kMaxRouteSquares = 32;        // a patrol route has 2..32 squares
constexpr int kMaxIgnores = 32;

// Reads a "behaviour" value: a mode name ("advance" | "hold" | "guard") or an object. Checks everything that needs no
// map (key names, combinations, ranges, unit type ids). Returns false with a plain-English `err` (no location).
bool parseBehaviour(const Json& v, const Database& db, AiBehaviour& out, std::string& err);
// The same block written as an object with only the keys that differ from the default (what saves use).
Json behaviourToJson(const AiBehaviour& b);
const char* behaviourModeName(AiMode m);

// ---- behaviour_scripted.cpp: the keys of the scripted-style computer players (order, weights, caution, retreat, idleTurns, route, ignores, measure, heals) ----
bool scriptedKey(const std::string& key);                                                    // is `key` one of them?
bool scriptedIsDefault(const AiBehaviour& b);                                                // none of them set
bool readScripted(const Json& v, const Database& db, AiBehaviour& b, std::string& err);      // after mode / within are read
void writeScripted(const AiBehaviour& b, Json& o);

}  // namespace ote
