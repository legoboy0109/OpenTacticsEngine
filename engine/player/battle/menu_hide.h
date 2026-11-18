// The "hide" option of the action menu (game.json "menuUnavailable"; docs/SRPG.md "Menu options"): which entries that cannot be
// used right now are left out. The rule: hidden when the reason is "nothing to use it on / nothing to do it with" (the core marks
// those options `noTarget`); an entry that is off for any other reason (a cooldown, no uses left, a rank, a full pack) stays greyed
// with its reason, so the player learns why. Free of SDL and of the scene so a unit test can use it.
#pragma once
#include <vector>

#include "battle/rescue_menu.h"
#include "core/battle/battle.h"

namespace ote {

// An ability entry: left out with "hide" when it is off only because there is no valid target.
inline bool abilityEntryHidden(bool hide, const AbilityOption& o) { return hide && !o.ok && o.noTarget; }
// An open entry of a door or chest: left out when the unit has no key for it (a full pack stays greyed).
inline bool placeEntryHidden(bool hide, const PlaceOption& o) { return hide && !o.ok && o.noTarget; }
// The one "Staff" entry: left out when EVERY staff carried is off only for want of anybody to use it on. One that is off
// for a rank or class reason keeps the entry greyed (with that reason).
bool staffEntryHidden(bool hide, const std::vector<StaffOption>& opts);
// Drop with no free square (the core's rescue menu marks it `noTarget`).
inline bool rescueEntryHidden(bool hide, const RescueEntry& e) { return hide && !e.enabled && e.noTarget; }

}  // namespace ote
