// Words about weapon ranks and stat caps for the player (docs/SRPG.md section 7 "Player"): shared by the unit details,
// the item screens and the battle view. Everything is a plain function of the Battle, so a unit test can read it.
#pragma once
#include <string>
#include <vector>

#include "core/battle/battle.h"

namespace ote {

struct InfoRow;

// "Sword": a weapon group's id with a capital.
std::string groupLabel(const std::string& group);
// "Needs rank B" or "Only Kell can wield this" for a weapon item the unit's class can wield but whose rank it lacks or
// whose character list does not name it; "" for everything else (a weapon it can wield, another kind of item).
std::string weaponBlockReason(const Battle& b, const Unit& u, const ItemStack& s);
// "Sword rank C": the label that floats over a unit that ranked up (a RankUp event).
std::string rankUpText(const Battle& b, const Event& e);
// One row per weapon group for the Stats page: "Sword rank", the rank's name, and the experience towards the next rank
// ("45/70", growth "use" only; "MAX" at the top). Empty in a game without ranks.
std::vector<InfoRow> rankRows(const Battle& b, const Unit& u);
// The level-up box line for a stat that is at its cap ("+1 Strength (max)", "Strength: max"); "" when it is not.
std::string capLine(const Battle& b, const Unit& u, int stat, int gain);

}  // namespace ote
