#include "core/data/battle_art.h"

namespace ote {

namespace {
const char* const kKeys[kBattleAnims] = {"stand", "attack", "crit", "ranged", "cast", "dodge", "hurt", "defeat",
                                         "critRanged", "dodgeRanged", "standRanged", "miss"};
}

const char* battleAnimKey(BattleAnimId id) { return kKeys[id]; }

bool battleAnimFromKey(const std::string& key, BattleAnimId& out) {
  for (int i = 0; i < kBattleAnims; ++i)
    if (key == kKeys[i]) {
      out = (BattleAnimId)i;
      return true;
    }
  return false;
}

int BattleAnim::frameAt(int t) const {
  if (frames.empty()) return 0;
  const int total = totalMs();
  if (t < 0) t = 0;
  if (loop && total > 0) t %= total;
  int acc = 0;
  for (size_t i = 0; i < frames.size(); ++i) {
    acc += ms[i];
    if (t < acc) return (int)i;
  }
  return (int)frames.size() - 1;
}

int BattleArt::findSet(const std::string& id) const {
  for (size_t i = 0; i < sets.size(); ++i)
    if (sets[i].id == id) return (int)i;
  return -1;
}

int BattleArt::findBackground(const std::string& id) const {
  for (size_t i = 0; i < backgrounds.size(); ++i)
    if (backgrounds[i].id == id) return (int)i;
  return -1;
}

int BattleArt::resolveSet(int character, int type, const std::string& group, int weapon, const std::string& side) const {
  int best = -1, bestScore = 0;
  for (const BattleAssign& a : assign) {
    if (a.character >= 0 && a.character != character) continue;
    if (a.type >= 0 && a.type != type) continue;
    if (!a.group.empty() && a.group != group) continue;
    if (a.weapon >= 0 && a.weapon != weapon) continue;
    if (!a.side.empty() && a.side != side) continue;
    // The level of the rule (see the header).
    int level;
    if (a.character >= 0) level = a.weapon >= 0 ? 9 : !a.group.empty() ? 8 : 7;
    else if (a.type >= 0) level = a.weapon >= 0 ? 6 : !a.group.empty() ? 5 : 2;
    else if (a.weapon >= 0 || !a.group.empty()) level = a.weapon >= 0 ? 4 : 3;
    else level = 1;  // a side alone
    const int score = level * 4 + (a.character >= 0 && a.type >= 0 ? 2 : 0) + (!a.side.empty() ? 1 : 0);
    if (score > bestScore) {
      bestScore = score;
      best = a.set;
    }
  }
  return best;
}

int BattleArt::resolveEffect(int weapon, const std::string& group) const {
  int byGroup = -1;
  for (size_t i = 0; i < effects.size(); ++i) {
    if (weapon >= 0 && effects[i].weapon == weapon) return (int)i;
    if (byGroup < 0 && effects[i].weapon < 0 && !group.empty() && effects[i].group == group) byGroup = (int)i;
  }
  return byGroup;
}

}  // namespace ote
