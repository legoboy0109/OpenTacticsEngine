// How a computer-controlled unit behaves (docs/AI.md): its movement stance, when it starts, its group and an optional
// favourite target. Pure data, shared by a map unit, an `addUnit` command, the `setBehaviour` story command and the live
// Unit (which also saves it). The default value is exactly the computer opponent's behaviour before this existed.
#pragma once
#include <string>

#include "core/base/json.h"

namespace ote {

struct Database;

enum class AiMode {
  Advance,    // default: walks toward the enemy and attacks
  Hold,       // never moves; attacks and uses staves on what it can reach from its square
  Guard,      // waits until a hostile unit is within its threat range, then acts like Advance (or stays reactive)
  GuardArea,  // waits until a hostile unit is inside an area or within N squares, then acts like Advance
  Seek        // heads for a named unit, a structure (place) or a square
};

// A soft preference between targets (a bonus on top of the usual scoring, never a veto).
enum class AiPrefer { None, Weakest, Nearest, Unit, Type, Tag };

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

  bool isDefault() const;
};

constexpr int kMaxBehaviourTurn = 99;    // "fromTurn" 1..99
constexpr int kMaxBehaviourWithin = 99;  // "within" 1..99

// Reads a "behaviour" value: a mode name ("advance" | "hold" | "guard") or an object. Checks everything that needs no
// map (key names, combinations, ranges, unit type ids). Returns false with a plain-English `err` (no location).
bool parseBehaviour(const Json& v, const Database& db, AiBehaviour& out, std::string& err);
// The same block written as an object with only the keys that differ from the default (what saves use).
Json behaviourToJson(const AiBehaviour& b);
const char* behaviourModeName(AiMode m);

}  // namespace ote
