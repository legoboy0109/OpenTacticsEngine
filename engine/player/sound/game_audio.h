// Everything about the sound of the game the player has open: its data/audio.json, the loaded sound effects, the
// cue lookup, the music director (docs/SOUND.md). One per PlayerApp (SceneHost::gameAudio()). With no game open,
// or a game without data/audio.json, every call does nothing, so scenes call it unconditionally.
//
// Presentation only: nothing here reads or changes a Battle, and the sound variants are picked by a counter,
// never by the rules' random numbers.
#pragma once
#include <string>
#include <vector>

#include "sound/audio_data.h"
#include "sound/cue_sink.h"
#include "sound/music_director.h"
#include "sound/sound_bank.h"

namespace ote {

class GameAudio {
 public:
  explicit GameAudio(AudioService& audio) : audio_(audio) {}

  // Loads the game's sound data. `db` null = only data/audio.json (the launcher's game page, which has no loaded
  // game: title music and the standard set); with the Database the overrides and the story ids are loaded and
  // checked too. False and a message in err when audio.json is wrong or a sound file is damaged: nothing is kept.
  // Loading the same game again (page, then session) keeps the music that plays. Off mode: nothing is read.
  bool load(const std::string& gameDir, const std::string& gameId, const Database* db, std::string& err);
  // Stops the music and forgets the game (the launcher's home screen).
  void unload();
  const std::string& gameId() const { return gameId_; }
  bool full() const { return full_; }  // loaded with a Database: overrides and story ids included
  bool loaded() const { return !gameId_.empty(); }

  // Once per frame, before the scenes run: opens a new "frame" for the identical-sound cap.
  void beginFrame();

  // Plays what `cue` means in this game: the first of weapon / item, unit type and the standard set that defines
  // it. Rate limits: the same sound at most once per frame, and a minimum gap for the cues that repeat fast.
  void cue(Cue cue, const CueCtx& ctx = CueCtx());
  // Does this game give `cue` a sound here (for a cue that falls back to another, like a critical hit to a hit)?
  bool defines(Cue cue, const CueCtx& ctx = CueCtx()) const { return data_.present && SoundBank::lookup(data_, cue, ctx) >= 0; }
  // A sound by its id (the story's playSound). Unknown ids are ignored with a warning.
  void sound(const std::string& id);

  MusicDirector& music() { return director_; }
  const AudioData& data() const { return data_; }
  const SoundBank& bank() const { return bank_; }

 private:
  void playSoundIndex(int index, int cueSlot);  // cueSlot -1: no minimum gap

  AudioService& audio_;
  std::string gameId_, dir_;
  AudioData data_;
  SoundBank bank_;
  MusicDirector director_;
  bool full_ = false;  // overrides and story ids are loaded (a Database was given)
  long long frame_ = 0;
  std::vector<long long> soundFrame_;  // per sound: the frame it last played in
  double lastCue_[kCueCount] = {};     // per cue: AudioService clock of the last play
  bool played_[kCueCount] = {};
};

// What uiCue() calls land on: the open game's cue lookup (installed by PlayerApp).
class GameAudioCues : public CueSink {
 public:
  explicit GameAudioCues(GameAudio& game) : game_(game) {}
  void onCue(Cue cue) override { game_.cue(cue); }

 private:
  GameAudio& game_;
};

}  // namespace ote
