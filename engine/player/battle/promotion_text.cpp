#include "battle/promotion_text.h"

#include <algorithm>

namespace ote {
namespace {

std::string withArticle(const std::string& noun) {
  bool vowel = !noun.empty() && std::string("aeiouAEIOU").find(noun[0]) != std::string::npos;
  return (vowel ? "an " : "a ") + noun;
}

// The entry the unit's type has for `to` that is on offer the way the player asked (item or offer), or null.
const PromotionDef* entryFor(const Battle& b, const Unit& u, int slot, int to) {
  int item = slot >= 0 && slot < (int)u.items.size() ? u.items[(size_t)slot].item : -1;
  for (const PromotionDef& d : b.typeOf(u).promotions) {
    if (d.to != to || u.level < b.promotionFromLevel(u, d)) continue;
    if (item >= 0 ? std::find(d.items.begin(), d.items.end(), item) != d.items.end() : d.atLevel == PromoteAtLevel::Offer)
      return &d;
  }
  return nullptr;
}

}  // namespace

PromotionPreview previewPromotion(const Battle& b, int unit, int slot, int to) {
  PromotionPreview p;
  const Database& db = *b.db;
  if (unit < 0 || unit >= (int)b.units.size() || to < 0 || to >= (int)db.unitTypes.size()) return p;
  const Unit& u = b.units[(size_t)unit];
  const UnitTypeDef& from = b.typeOf(u);
  const UnitTypeDef& next = db.unitTypes[(size_t)to];
  const PromotionDef* def = entryFor(b, u, slot, to);
  p.to = to;
  p.name = next.name;

  for (size_t s = 0; s < u.stats.size() && s < db.stats.size(); ++s) {
    int gain = def && def->hasGains && s < def->gains.size() ? def->gains[s] : next.stats[s] - from.stats[s];
    int change = std::max(0, u.stats[s] + gain) - u.stats[s];  // a stat stops at 0
    if (change != 0) p.changes.push_back((change > 0 ? "+" : "") + std::to_string(change) + " " + db.stats[s].name);
  }

  LevelAfter rule = def ? def->levelAfter : LevelAfter::Game;
  if (rule == LevelAfter::Game) rule = db.rules.promotion.keepLevel ? LevelAfter::Keep : LevelAfter::Reset;
  const bool reset = rule == LevelAfter::Reset;
  if (reset && u.level != 1) p.level = "Back to level 1";

  // Skills the new type adds: its own that the old type did not have, and those it learns by the level it will have.
  const int newLevel = reset ? 1 : u.level;
  auto add = [&](int skill) {
    if (skill < 0 || skill >= (int)db.skills.size()) return;
    const std::string& n = db.skills[(size_t)skill].name;
    if (std::find(p.skills.begin(), p.skills.end(), n) == p.skills.end()) p.skills.push_back(n);
  };
  for (int s : next.skills)
    if (std::find(from.skills.begin(), from.skills.end(), s) == from.skills.end()) add(s);
  for (const LearnDef& l : next.learns)
    if (l.level <= newLevel) add(l.skill);

  if (db.rules.items.carriedWeapons)
    for (const ItemStack& st : u.items) {
      const ItemDef& d = db.items[(size_t)st.item];
      if (d.kind == ItemKind::Weapon && db.canWield(u.type, d.weapon, u.character) && !db.canWield(to, d.weapon, u.character)) p.cannotWield.push_back(d.name);
    }
  return p;
}

std::string joinChanges(const std::vector<std::string>& changes) {
  if (changes.empty()) return "No stat changes";
  std::string out;
  for (const std::string& c : changes) out += (out.empty() ? "" : ", ") + c;
  return out;
}

std::string promotionQuestion(const Battle& b, int unit, const PromotionPreview& p) {
  std::string q = "Promote " + b.units[(size_t)unit].name + " to " + p.name + "?\n" + joinChanges(p.changes);
  if (!p.level.empty()) q += "\n" + p.level;
  return q;
}

std::string promotionBannerText(const std::string& unitName, const std::string& typeName) {
  return unitName + " is now " + withArticle(typeName);
}

}  // namespace ote
