#include "sound/sound_bank.h"

#include <cstdio>

#include "core/base/log.h"

namespace ote {
namespace {

bool fileExists(const std::string& path) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  std::fclose(f);
  return true;
}

}  // namespace

void SoundBank::clear() {
  ids_.clear();
  next_.clear();
  musicOk_.clear();
}

bool SoundBank::load(AudioService& audio, const std::string& dir, const AudioData& data, std::string& err) {
  clear();
  ids_.resize(data.sounds.size());
  next_.assign(data.sounds.size(), 0);
  for (size_t i = 0; i < data.sounds.size(); ++i) {
    for (const std::string& rel : data.sounds[i].files) {
      const std::string path = dir + "/" + rel;
      if (!fileExists(path)) {
        OTE_WARN("audio: sound '%s': the file %s does not exist; that sound stays silent", data.sounds[i].id.c_str(), path.c_str());
        continue;
      }
      std::string lerr;
      SoundId id = audio.loadSound(path, lerr);
      if (id == 0) {
        err = dir + "/data/audio.json: sound \"" + data.sounds[i].id + "\": " + lerr;
        clear();
        return false;
      }
      ids_[i].push_back(id);
    }
  }
  musicOk_.assign(data.music.size(), true);
  for (size_t i = 0; i < data.music.size(); ++i) {
    const std::string path = dir + "/" + data.music[i].file;
    if (!fileExists(path)) {
      OTE_WARN("audio: music '%s': the file %s does not exist; that track stays silent", data.music[i].id.c_str(), path.c_str());
      musicOk_[i] = false;
    }
  }
  return true;
}

int SoundBank::lookup(const AudioData& d, Cue cue, const CueCtx& ctx) {
  const int c = (int)cue;
  // Weapon, then item (they never both apply to one cue), then the unit type, its movement type, then the standard set. kSilent in a
  // layer ends the search with nothing; kNone passes it on.
  const int layers[5] = {
      ctx.weapon >= 0 && (size_t)ctx.weapon < d.weapons.size() ? d.weapons[(size_t)ctx.weapon].sound[c] : kNone,
      ctx.item >= 0 && (size_t)ctx.item < d.items.size()
          ? (cue == Cue::Heal && ctx.scene && d.items[(size_t)ctx.item].healScene != kNone ? d.items[(size_t)ctx.item].healScene : d.items[(size_t)ctx.item].sound[c])
          : kNone,
      ctx.unitType >= 0 && (size_t)ctx.unitType < d.units.size() ? d.units[(size_t)ctx.unitType].sound[c] : kNone,
      ctx.moveType >= 0 && (size_t)ctx.moveType < d.moveTypes.size() ? d.moveTypes[(size_t)ctx.moveType].sound[c] : kNone,
      d.standard.sound[c]};
  for (int s : layers) {
    if (s == kSilent) return -1;
    if (s >= 0) return s;
  }
  return -1;
}

SoundId SoundBank::pick(int index) {
  if (index < 0 || (size_t)index >= ids_.size() || ids_[(size_t)index].empty()) return 0;
  const std::vector<SoundId>& list = ids_[(size_t)index];
  size_t& n = next_[(size_t)index];
  SoundId id = list[n % list.size()];
  n = (n + 1) % list.size();
  return id;
}

}  // namespace ote
