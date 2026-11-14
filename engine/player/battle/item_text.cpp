#include "battle/item_text.h"

#include "ui/text.h"

namespace ote {

std::string usesText(const Battle& b, const ItemStack& s) {
  const ItemDef& d = b.itemOf(s);
  if (s.uses < 0 || d.uses < 0) return "";
  return intStr(s.uses) + "/" + intStr(d.uses);
}

std::string gainsText(const Database& db, const std::vector<int>& perStat) {
  std::string out;
  for (size_t i = 0; i < perStat.size() && i < db.stats.size(); ++i) {
    if (perStat[i] == 0) continue;
    if (!out.empty()) out += " ";
    out += (perStat[i] > 0 ? "+" : "") + intStr(perStat[i]) + " " + db.stats[i].name;
  }
  return out;
}

std::string itemDetail(const Battle& b, const ItemStack& s) {
  const ItemDef& d = b.itemOf(s);
  if (d.kind == ItemKind::Accessory) return gainsText(*b.db, d.bonus);
  if (!d.description.empty()) return d.description;
  if (d.kind == ItemKind::Weapon && d.weapon >= 0) {
    const WeaponDef& w = b.db->weapons[(size_t)d.weapon];
    return "Range " + (w.minRange == w.maxRange ? intStr(w.minRange) : intStr(w.minRange) + "-" + intStr(w.maxRange));
  }
  return "";
}

std::string priceText(const Database& db, const std::vector<Amount>& price) {
  std::string out;
  for (const Amount& a : price)
    out += (out.empty() ? "" : " ") + intStr(a.amount) + " " + db.resources[(size_t)a.resource].name;
  return out;
}

}  // namespace ote
