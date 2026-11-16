#include "battle/rank_text.h"

#include "battle/unit_info_rows.h"
#include "ui/text.h"

namespace ote {

std::string groupLabel(const std::string& group) {
  std::string s = group;
  if (!s.empty() && s[0] >= 'a' && s[0] <= 'z') s[0] = (char)(s[0] - 'a' + 'A');
  return s;
}

std::string weaponBlockReason(const Battle& b, const Unit& u, const ItemStack& s) {
  const ItemDef& d = b.itemOf(s);
  if (d.kind != ItemKind::Weapon || !b.db->canWield(u.type, d.weapon)) return "";
  std::string why;  // the character lock ("Only Kell can wield this") or the rank ("Needs rank B")
  return b.canWieldNow(u, d.weapon, &why) ? "" : why;
}

std::string rankUpText(const Battle& b, const Event& e) {
  const RankRules& r = b.db->ranks;
  const std::string group = e.b >= 0 && (size_t)e.b < r.groups.size() ? groupLabel(r.groups[(size_t)e.b]) : "Weapon";
  const std::string rank = e.c >= 0 && (size_t)e.c < r.ladder.size() ? r.ladder[(size_t)e.c].name : "?";
  return group + " rank " + rank;
}

std::vector<InfoRow> rankRows(const Battle& b, const Unit& u) {
  std::vector<InfoRow> rows;
  const RankRules& r = b.db->ranks;
  if (!r.on()) return rows;
  const Color text(228, 230, 240), label(150, 158, 190), gold(255, 225, 110);
  for (size_t g = 0; g < r.groups.size(); ++g) {
    const int xp = g < u.weaponXp.size() ? u.weaponXp[g] : 0;
    const int rank = r.rankAt(xp);
    InfoRow row{groupLabel(r.groups[g]) + " rank", r.ladder[(size_t)rank].name, "", text, label};
    if (r.growByUse) {
      const bool top = (size_t)rank + 1 >= r.ladder.size();
      row.note = top ? "MAX" : intStr(xp) + "/" + intStr(r.ladder[(size_t)rank + 1].xp);
      if (top) row.noteColor = gold;
    }
    rows.push_back(row);
  }
  return rows;
}

std::string capLine(const Battle& b, const Unit& u, int stat, int gain) {
  const int cap = b.statCap(u, stat);
  if (cap < 0 || stat < 0 || (size_t)stat >= u.stats.size() || u.stats[(size_t)stat] < cap) return "";
  const std::string& name = b.db->stats[(size_t)stat].name;
  return gain > 0 ? "+" + intStr(gain) + " " + name + " (max)" : name + ": max";
}

}  // namespace ote
