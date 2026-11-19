#include "sound/cues.h"

namespace ote {
namespace {

const char* const kKeys[kCueCount] = {"cursor", "confirm", "cancel", "error", "step", "hit", "miss", "critical", "heal",
                                      "defeated", "capture", "build", "itemUse", "gold", "dialogue", "turnStart",
                                      "upgrade", "research", "repair", "supply", "ambush", "select", "levelUpTick", "save"};

struct Entry { const char* key; Cue cue; };
const Entry kWeapon[] = {{"hit", Cue::Hit}, {"miss", Cue::Miss}, {"critical", Cue::Critical}};
const Entry kUnit[] = {{"step", Cue::Step}, {"hit", Cue::Hit}, {"miss", Cue::Miss}, {"critical", Cue::Critical},
                       {"defeated", Cue::Defeated}, {"select", Cue::Select}, {"heal", Cue::Heal}};
const Entry kItem[] = {{"use", Cue::ItemUse}, {"heal", Cue::Heal}};  // "heal": the "+N" cue of this staff / healing item (docs/SOUND.md)
const Entry kMoveType[] = {{"step", Cue::Step}};  // docs/SOUND.md "Step sounds by movement type"

template <size_t N>
bool find(const Entry (&list)[N], const std::string& key, Cue& out) {
  for (const Entry& e : list)
    if (key == e.key) { out = e.cue; return true; }
  return false;
}

template <size_t N>
std::string keys(const Entry (&list)[N]) {
  std::string s;
  for (const Entry& e : list) s += (s.empty() ? "\"" : ", \"") + std::string(e.key) + "\"";
  return s;
}

}  // namespace

const char* cueKey(Cue c) { return kKeys[(int)c]; }

bool cueFromKey(const std::string& key, Cue& out) {
  for (int i = 0; i < kCueCount; ++i)
    if (key == kKeys[i]) { out = (Cue)i; return true; }
  return false;
}

bool overrideCue(OverrideScope scope, const std::string& key, Cue& out) {
  switch (scope) {
    case OverrideScope::Weapon: return find(kWeapon, key, out);
    case OverrideScope::Unit: return find(kUnit, key, out);
    case OverrideScope::Item: return find(kItem, key, out);
    case OverrideScope::MoveType: return find(kMoveType, key, out);
  }
  return false;
}

std::string overrideKeys(OverrideScope scope) {
  if (scope == OverrideScope::Item) return keys(kItem) + ", \"healScene\"";  // (healScene is read by readBlock: it has no cue of its own)
  switch (scope) {
    case OverrideScope::Weapon: return keys(kWeapon);
    case OverrideScope::Unit: return keys(kUnit);
    case OverrideScope::Item: return keys(kItem);
    case OverrideScope::MoveType: return keys(kMoveType);
  }
  return "";
}

}  // namespace ote
