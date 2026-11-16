#include "battle/staff_text.h"

#include <algorithm>

#include "ui/text.h"

namespace ote {

std::string staffReasonText(const std::string& why, int form) {
  if (why == "nobody in reach needs that") return form == 0 ? StaffWords::kNobody : form == 1 ? StaffWords::kNobodyShort : StaffWords::kNobodyTiny;
  if (why == "the unit cannot wield that") return form >= 2 ? "Class" : StaffWords::kCannotWield;
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
    if (fx.kind != StaffEffect::Kind::Heal) continue;
    switch (fx.amount) {
      case StaffEffect::Amount::Fixed: return "Heals " + intStr(fx.value) + " HP";
      case StaffEffect::Amount::Full: return "Heals all HP";
      case StaffEffect::Amount::Percent: return "Heals " + intStr(fx.value) + "%";
      case StaffEffect::Amount::Formula: return "Heals";
    }
  }
  return "";
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
