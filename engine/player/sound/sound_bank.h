// The loaded sound effects of one game and the lookup "which sound does this cue play" (docs/SOUND.md):
//   weapon override / item override, then the unit type's, then its movement type's, then the game's standard set, then silence.
// A sound with several files plays them in turn (a fixed sequence, never the rules' random numbers: audio must
// not touch the game's RNG, and a replay must sound the same).
#pragma once
#include <string>
#include <vector>

#include "sound/audio_data.h"
#include "sound/audio_service.h"

namespace ote {

// What a cue is about, for the lookup: an index into db.weapons / db.items / db.unitTypes, -1 = none.
struct CueCtx {
  int weapon = -1, item = -1, unitType = -1, moveType = -1;  // moveType: index into db.moveTypes (the step sound of a movement type)
  bool scene = false;  // the cue plays in the battle scene: an item's "healScene" then replaces its "heal" (docs/SOUND.md "A staff's own heal sound")
};

class SoundBank {
 public:
  // Reads and decodes every sound file through `audio` (cached there by path). A file that does not exist is a
  // warning and that sound stays silent; a file that exists but is damaged or over the limits is an error (false,
  // message in err). Music files are only checked for existing (warning), they are opened when played.
  bool load(AudioService& audio, const std::string& dir, const AudioData& data, std::string& err);
  void clear();

  // The index into data.sounds that `cue` plays here, -1 = nothing. Pure: no state changes.
  static int lookup(const AudioData& data, Cue cue, const CueCtx& ctx);

  // Picks the next file of sound `index` (round robin). 0 when the sound has no usable file.
  SoundId pick(int index);
  bool musicFileOk(int index) const { return index >= 0 && (size_t)index < musicOk_.size() && musicOk_[(size_t)index]; }

 private:
  std::vector<std::vector<SoundId>> ids_;  // per sound: the loaded files (missing ones left out)
  std::vector<size_t> next_;               // per sound: which one comes next
  std::vector<bool> musicOk_;              // per track: its file exists
};

}  // namespace ote
