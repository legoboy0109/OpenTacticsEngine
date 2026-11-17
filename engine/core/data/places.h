// Doors, chests and visitable places (contract: docs/SRPG.md section 10). Included by database.h before StructureDef.
//
// They are STRUCTURES (docs/STRUCTURES.md): a structure kind with a "door", "chest" or "visit" block. The kind carries what
// the thing does; opening, shutting or destroying it changes the structure's kind (the engine's existing upgrade mechanism:
// footprint, movement cost, art, fog memory and saves all follow the kind). Only this file's few new numbers are state.
#pragma once
#include <string>
#include <vector>

namespace ote {

enum class PlaceKind { None, Door, Chest, Visit };

// One thing inside a chest (a map placement's "contents"): an item or an amount of a resource.
struct PlaceLoot {
  int item = -1;      // index into Database::items; -1 = a resource
  int uses = -1;      // item: uses it comes with (-1 = unlimited)
  int resource = -1;  // index into Database::resources
  int amount = 0;     // resource: how much
};

// The "door" / "chest" / "visit" block of a structure kind.
struct PlaceDef {
  PlaceKind kind = PlaceKind::None;
  // Door / chest: the kind it becomes when opened. Visit: the kind it becomes after a visit ("shut"); -1 = it stays.
  int to = -1;
  // Visit only: the kind it becomes when destroyed; -1 = it cannot be destroyed.
  int destroyedTo = -1;
  bool locked = true;        // door / chest: starts locked (a map placement may say otherwise)
  std::vector<int> keys;     // door / chest: the item indices that fit; empty = any item whose "opens" names this kind
  bool byKey = true;         // ways to open it (door / chest): a key item,
  bool byTrait = true;       //   a unit's unlock ability,
  bool byBreak = false;      //   being broken down (door, needs hp). A story command and an unlock staff always work.
  int hp = 0;                // door: hit points; 0 = cannot be attacked
  std::vector<int> stats;    // door: per stat index the numbers the damage formula reads as `b.<stat>` (defence...)
  bool fromOn = true;        // from which squares a unit may interact: on a square of it,
  bool fromNext = true;      //   next to it. (Both true = either.)
  bool repeat = false;       // visit: may be visited again and again
  bool isDoor() const { return kind == PlaceKind::Door; }
  bool isChest() const { return kind == PlaceKind::Chest; }
  bool isVisit() const { return kind == PlaceKind::Visit; }
};

// game.json "rules.places": does opening a chest, visiting and opening a door with a key need an explicit menu action
// (default), or does it happen by itself when a unit ends its move there?
struct PlaceRules {
  bool automatic = false;
};

// map unit / addUnit "goes for": what the computer opponent does with these (docs/SRPG.md section 10 "Computer opponent").
enum : int { kAiChests = 1, kAiPlaces = 2, kAiDoors = 4 };

const int kMaxPlaceHp = 9999;
const int kMaxChestLoot = 8;

}  // namespace ote
