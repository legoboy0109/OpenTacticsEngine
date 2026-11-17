#include "core/data/battle_art.h"

namespace ote {

namespace {
const char* const kKeys[kBattleAnims] = {"stand", "attack", "crit", "ranged", "cast", "dodge", "hurt", "defeat"};
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

int BattleArt::resolveSet(int character, int type, const std::string& group) const {
  int best = -1, bestScore = 0;
  for (const BattleAssign& a : assign) {
    if (a.character >= 0 && a.character != character) continue;
    if (a.type >= 0 && a.type != type) continue;
    if (!a.group.empty() && a.group != group) continue;
    // character 4 + group 2 + class 1: a character outranks a group, which outranks the class alone.
    const int score = (a.character >= 0 ? 4 : 0) + (!a.group.empty() ? 2 : 0) + (a.type >= 0 ? 1 : 0);
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
