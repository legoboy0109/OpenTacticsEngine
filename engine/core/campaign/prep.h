// The preparation screen's rules (docs/EVENTS.md "Preparation"): who fights, where they start, what they carry and
// what the preparation shop sells, all decided BEFORE the battle's `start`. Free functions over a Playthrough (the
// roster, what the side carries and PrepState) and the chapter's MapDef; nothing here needs a Battle. Every action
// returns false with a plain sentence in `why` when it is refused, and then changes nothing.
//
// A "candidate" is a character that may be deployed: every living roster unit, plus the named units of the prep
// side that the map itself places and the roster does not know yet (their placed square is ignored: the slots decide
// where they start). Placed units that stay put (computer units, unnamed units, other sides, the map's "fixed" list)
// are not candidates and are not part of the choice.
//
// Free battles (no campaign) use the same calls with a scratch Playthrough (default-constructed): the candidates are
// the map's own units, and items and the shop are simply not offered by the player.
#pragma once
#include <string>
#include <vector>

#include "core/campaign/playthrough.h"
#include "core/data/prep.h"

namespace ote {

enum class PrepLock {
  None,       // free to deploy or bench
  Required,   // always deployed (the map says so)
  Forbidden,  // cannot take part in this map
  Fallen,     // gone for good ("fallen": "gone")
};

struct PrepEntry {
  std::string id, name;
  int type = 0, level = 1;
  bool inRoster = false;  // false: only the map knows it so far
  PrepLock lock = PrepLock::None;
  int slot = -1;          // deployed on this slot (index into PrepDef::slots), -1 = benched
  int fixedSlot = -1;     // Required with a slot of its own
};

// ---- reading ----
bool prepOn(const MapDef& map);
// The candidates: roster order first, then the map's own, each with its lock and where it is deployed now.
std::vector<PrepEntry> prepEntries(const Database& db, const Playthrough& pt, const MapDef& map);
// The default pick: required characters, then those deployed last chapter (in that order), then the rest in order,
// up to the limit; slots in order (a required character's own slot first). What the simulator, --autoplay and
// Play-without-interaction use. Deterministic.
std::vector<PrepDeploy> prepDefaultDeployment(const Database& db, const Playthrough& pt, const MapDef& map);
// The deployment a battle would start with now: pt.prep.deployed, or the default pick when nothing was chosen.
std::vector<PrepDeploy> prepCurrentDeployment(const Database& db, const Playthrough& pt, const MapDef& map);
int prepDeployedCount(const Playthrough& pt);
// Can a unit of `type` stand on slot `slot`? (the terrain and the structure there, for its movement type)
bool prepCanStand(const Database& db, const MapDef& map, int type, int slot);
// Why a character is locked in or out, in words ("" = it is free).
std::string prepLockReason(PrepLock lock);
// The slot's index at square x,y (-1 none).
int prepSlotAt(const MapDef& map, int x, int y);

// ---- the screen's state ----
// Opens the screen and makes the default pick (unless a choice is already there: after a lost battle, or in a save).
// `carry` = a campaign: the map's own candidates the roster does not know enter the roster (so their items can be
// traded) and the prep side's map allowance (resources, storage) is folded into what it carries. A free battle passes
// false with a scratch Playthrough. Calling it while open changes nothing.
void prepOpen(const Database& db, Playthrough& pt, const MapDef& map, bool carry);
// Is a deployment that came from outside (a save) possible on this map? False with the reason.
bool prepValidate(const Database& db, const Playthrough& pt, const MapDef& map, std::string& err);

// ---- who fights and where ----
// Deploys `id` on `slot` (-1 = the first free slot it can stand on).
bool prepDeploy(const Database& db, Playthrough& pt, const MapDef& map, const std::string& id, int slot, std::string& why);
bool prepBench(const Database& db, Playthrough& pt, const MapDef& map, const std::string& id, std::string& why);
// Moves a deployed character to `slot`; a character standing there takes its place (both must be able to stand).
bool prepMove(const Database& db, Playthrough& pt, const MapDef& map, const std::string& id, int slot, std::string& why);
// Begin: the choice is final and the screen closes. Refused with nobody deployed while someone could be.
bool prepBegin(const Database& db, Playthrough& pt, const MapDef& map, std::string& why);

// ---- items (campaign only: they live in the roster) ----
// An item from one character's inventory (position `slot`) to another's.
bool prepGive(const Database& db, Playthrough& pt, const MapDef& map, const std::string& from, int slot, const std::string& to,
              std::string& why);
// Into the shared storage / out of it (index into the list the screen shows = prepStorage).
bool prepStore(const Database& db, Playthrough& pt, const MapDef& map, const std::string& id, int slot, std::string& why);
bool prepFetch(const Database& db, Playthrough& pt, const MapDef& map, const std::string& id, int storageIndex, std::string& why);
// Makes the weapon at `slot` the first item (the one used).
bool prepEquip(const Database& db, Playthrough& pt, const MapDef& map, const std::string& id, int slot, std::string& why);
// The shared storage the screen shows (the prep side's), and what the prep side has to spend.
const std::vector<ItemGrant>& prepStorage(const Playthrough& pt, const MapDef& map);
std::vector<int> prepResources(const Database& db, const Playthrough& pt, const MapDef& map);
// Buy `item` (index into Database::items) at the preparation shop for `id`'s inventory, or into the storage when it is
// full (or `id` is empty) and the game has one.
bool prepBuy(const Database& db, Playthrough& pt, const MapDef& map, const std::string& id, int item, std::string& why);

// ---- starting the battle ----
// The map as the battle must start it: the deployed characters on their slots, the others left out. Identity for a map
// without preparation.
void prepApplyToMap(const Database& db, const Playthrough& pt, const MapDef& in, MapDef& out);
// After a won chapter (finishChapterBattle): the roster takes in the map's own candidates that were benched, the
// deployed ids are remembered, and the screen's state is cleared. `battle` is the finished battle.
void prepFinishChapter(const Database& db, Playthrough& pt, const MapDef& map, const Battle& battle);

}  // namespace ote
