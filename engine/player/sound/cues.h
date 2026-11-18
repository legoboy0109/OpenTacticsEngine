// The sound "cues": the things a game can give a sound to (docs/SOUND.md, `use.sounds`). A cue is what happens;
// which sound it plays is decided by the game's data (audio_data.h) and the lookup order in sound_bank.h.
#pragma once
#include <string>

namespace ote {

enum class Cue {
  // menus and screens
  Cursor, Confirm, Cancel, Error,
  // battle
  Step, Hit, Miss, Critical, Heal, Defeated, Capture, Build, ItemUse, Gold, Dialogue, TurnStart, Upgrade, Research,
  Repair, Supply,
  // optional extras (a game that does not define them hears nothing, except Select, which falls back to Confirm)
  Ambush, Select,
  // fidelity 3: one tick per stat that rises in the level-up box; a save written from a menu
  LevelUpTick, Save,
  Count
};
constexpr int kCueCount = (int)Cue::Count;

// The key in `use.sounds` of data/audio.json ("cursor", "itemUse", ...).
const char* cueKey(Cue c);
bool cueFromKey(const std::string& key, Cue& out);

// Where an override block sits. The keys each scope accepts are listed in docs/SOUND.md; `key` is the JSON key
// inside the block's "sounds" object ("use" for an item is the cue ItemUse).
enum class OverrideScope { Weapon, Unit, Item, MoveType };
bool overrideCue(OverrideScope scope, const std::string& key, Cue& out);
// The accepted keys of a scope as text for an error message: "hit", "miss", "critical".
std::string overrideKeys(OverrideScope scope);

}  // namespace ote
