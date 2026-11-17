#include "battle/staff_text.h"

#include <algorithm>

#include "battle/condition_text.h"
#include "ui/text.h"

namespace ote {

std::string staffReasonText(const std::string& why, int form) {
  if (why == "nobody in reach needs that") return form == 0 ? StaffWords::kNobody : form == 1 ? StaffWords::kNobodyShort : StaffWords::kNobodyTiny;
  if (why == "the unit cannot wield that") return form >= 2 ? "Class" : StaffWords::kCannotWield;
  // Stage 2: what a staff of another kind finds nobody for, and a light's reasons.
  if (why == "nobody in reach can be moved") return form == 0 ? "Nobody in reach can be moved" : form == 1 ? "Nobody to move" : StaffWords::kNobodyTiny;
  if (why == "nobody in reach has an item to repair") return form == 0 ? "Nothing in reach to repair" : form == 1 ? "Nothing to repair" : StaffWords::kNobodyTiny;
  if (why == "this map has no fog of war") return form == 0 ? "No fog on this map" : form == 1 ? "No fog here" : "No fog";
  if (why == "nothing in reach is hidden") return form == 0 ? "Nothing in reach is hidden" : form == 1 ? "Nothing hidden" : "None";
  // Stage 3 (conditions).
  if (why == "nobody in reach has anything it cures") return form == 0 ? "Nobody in reach to cure" : form == 1 ? "Nobody to cure" : StaffWords::kNobodyTiny;
  if (why == "nobody in reach can be affected") return form == 0 ? "Nobody in reach it can affect" : form == 1 ? "Nobody to affect" : StaffWords::kNobodyTiny;
  static const std::string kSuffix = " can wield this", kNeeds = "Needs rank ";
  if (form >= 1 && why.compare(0, 5, "Only ") == 0 && why.size() > kSuffix.size() &&
      why.compare(why.size() - kSuffix.size(), kSuffix.size(), kSuffix) == 0)
    return form == 1 ? why.substr(0, why.size() - kSuffix.size()) : "Locked";  // "Only Mara"
  if (form >= 2 && why.compare(0, kNeeds.size(), kNeeds) == 0) return "Rank " + why.substr(kNeeds.size());
  return why;
}

std::string staffEffectText(const Database& db, const ItemDef& item) {
  if (item.kind != ItemKind::Staff || item.staff < 0) return "";
  for (const StaffEffect& fx : db.staves[(size_t)item.staff].effects) {
    switch (fx.kind) {
      case StaffEffect::Kind::Warp: return "Warps a unit up to " + intStr(fx.warpMax) + " squares away";
      case StaffEffect::Kind::Pull: return "Pulls a unit next to you";
      case StaffEffect::Kind::Swap: return "Swaps places";
      case StaffEffect::Kind::Repair: return fx.amount == StaffEffect::Amount::Full ? "Repairs an item fully" : "Repairs " + intStr(fx.value) + " uses";
      case StaffEffect::Kind::Light: return "Lights an area";
      case StaffEffect::Kind::Unlock:  // docs/SRPG.md section 10 (placeholder wording)
        return fx.unlockDoors && fx.unlockChests ? "Opens a door or chest" : fx.unlockDoors ? "Opens a door" : "Opens a chest";
      case StaffEffect::Kind::Inflict: return "Inflicts " + conditionName(db, fx.conds.empty() ? -1 : fx.conds[0]);
      case StaffEffect::Kind::Cure: {
        if (fx.cureAll) return "Cures every condition";
        std::string names;
        for (size_t i = 0; i < fx.conds.size(); ++i) names += (i ? ", " : "") + conditionName(db, fx.conds[i]);
        return "Cures " + names;
      }
      case StaffEffect::Kind::Heal: break;
    }
    switch (fx.amount) {
      case StaffEffect::Amount::Fixed: return "Heals " + intStr(fx.value) + " HP";
      case StaffEffect::Amount::Full: return "Heals all HP";
      case StaffEffect::Amount::Percent: return "Heals " + intStr(fx.value) + "%";
      case StaffEffect::Amount::Formula: return "Heals";
    }
  }
  return "";
}

std::string staffWhyText(const std::string& why) {
  struct Row { const char* core; const char* words; };
  static const Row rows[] = {
      {"that unit is out of range", StaffWords::kOutOfRange},
      {"that staff does not work on that unit", StaffWords::kNotAValidTarget},
      {"you cannot see that unit", StaffWords::kCannotSee},
      {"nobody in reach needs that", StaffWords::kNothingToDo},
      {"that unit is already next to the user", StaffWords::kAlreadyBeside},
      {"there is no free square next to the user that it could stand on", StaffWords::kNoRoomBeside},
      {"there is no free square in reach of the user that it could stand on", StaffWords::kNoFreeSquare},
      {"they could not stand on each other's squares", StaffWords::kCannotStandThere},
      {"that unit carries nothing that needs repair", StaffWords::kNothingToRepair},
      {"a unit cannot be pulled to itself", StaffWords::kNotAValidTarget},
      {"a unit cannot swap with itself", StaffWords::kNotAValidTarget},
      {"that unit is immune to it", ConditionWords::kImmune},
      {"that unit has it already", ConditionWords::kAlreadyAffected},
      {"that unit has nothing it cures", ConditionWords::kNothingToCure},
  };
  for (const Row& r : rows)
    if (why == r.core) return r.words;
  return why.empty() ? std::string() : why;
}

std::vector<std::string> staffMoveLines(const Battle& b, const StaffForecast& f, const StaffHit& h) {
  std::vector<std::string> out;
  const std::string name = b.units[(size_t)h.unit].name;
  if (h.moveTo != kNoCell && h.userTo != kNoCell && f.user >= 0) out.push_back(name + " and " + b.units[(size_t)f.user].name + " " + StaffWords::kSwapPlaces);
  else if (h.moveTo != kNoCell && f.needsDest == false && b.db->items[(size_t)f.item].staff >= 0 &&
           b.db->staves[(size_t)b.db->items[(size_t)f.item].staff].has(StaffEffect::Kind::Warp))
    out.push_back(name + " " + StaffWords::kWarpsThere);
  else if (h.moveTo != kNoCell) out.push_back(name + " " + StaffWords::kPulledToYou);
  if (f.needsDest) out.push_back(name + ": " + StaffWords::kPickSquareNote);
  if (h.repairItem >= 0 && h.repairItem < (int)b.db->items.size())
    out.push_back(b.db->items[(size_t)h.repairItem].name + "  " + intStr(h.usesBefore) + " > " + intStr(h.usesAfter));
  else if (f.needsItem) out.push_back(StaffWords::kPickItemNote);
  return out;
}

std::vector<std::string> staffLightLines(const StaffForecast& f) {
  std::vector<std::string> out;
  if (!f.isLight) return out;
  out.push_back(std::string(StaffWords::kLights) + " " + intStr(f.newSquares) + " " + (f.newSquares == 1 ? StaffWords::kNewSquare : StaffWords::kNewSquares));
  out.push_back(intStr(f.lightTurns) + " " + (f.lightTurns == 1 ? StaffWords::kTurn : StaffWords::kTurns));
  return out;
}

std::string staffHitText(const StaffForecast& f) {
  if (!f.canMiss || f.hits.empty()) return "";
  int lo = 100, hi = 0;
  for (const StaffHit& h : f.hits) {
    lo = std::min(lo, h.hit);
    hi = std::max(hi, h.hit);
  }
  return "Hit " + (lo == hi ? intStr(lo) : intStr(lo) + "-" + intStr(hi)) + "%";
}

}  // namespace ote
