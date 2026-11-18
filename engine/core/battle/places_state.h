// The small types of doors, chests and visiting (docs/SRPG.md section 10) that battle.h needs before the class.
#pragma once
#include <string>
#include <vector>

#include "core/base/types.h"

namespace ote {

// One thing a unit could do to a door, chest or place from where it stands (Battle::placeOptions): one entry per structure
// and verb. A menu shows `ok` ones and may show the others greyed with `why`.
struct PlaceOption {
  enum class Verb { Open, Visit, Destroy };
  int structure = -1;
  Verb verb = Verb::Open;
  // Open: the inventory slots of the keys that fit, in inventory order (the first is the default). Empty = it needs none (it is
  // not locked) or, when `ok` is false, nothing fits.
  std::vector<int> keySlots;
  bool locked = false;  // Open: the door / chest is locked
  bool ok = false;
  std::string why;      // when !ok: "needs a key", "the unit cannot carry more", ...
  bool noTarget = false;  // !ok only because the unit has nothing to open it with (no key): the "hide" menu option leaves it out
};

// A door a unit could attack from where it stands with a weapon (Battle::placeAttackOptions).
struct PlaceAttack {
  int structure = -1;
  int weapon = -1;
};

// What attacking a door would do (Battle::placeForecast). There is no dodge and no counter: a door only takes damage.
struct PlaceForecast {
  int structure = -1;
  int weapon = -1;
  int damage = 0;       // per strike
  int strikes = 1;
  int total = 0;        // damage * strikes, at most hpBefore
  int hpBefore = 0, hpAfter = 0;
  bool breaks = false;  // the door would open
};

// Why Battle::placeKeySlots found nothing, for menus.
enum class KeyWhy { Fits, NoKey, NotLocked };

}  // namespace ote
