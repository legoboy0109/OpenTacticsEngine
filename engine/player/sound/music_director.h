// Decides which music should be playing and tells the AudioService (docs/SOUND.md "Music rules in the player").
// Priority, highest first: a jingle while it plays (the track under it pauses and resumes); music the story asked
// for with a command; the turn phase's music (player's / enemy's turn); the map's music; the game's default.
// A change of track cross-fades 0.5 s; the same track asked again keeps playing; every request is only a
// "this is what I want now", so the director can be told the same thing every frame.
//
// It works on track indices of AudioData::music and never on the rules: the battle scene and the campaign scenes
// tell it what is going on (enter, phase, story, jingle), nothing here knows about Battle.
#pragma once
#include <string>

#include "sound/audio_data.h"
#include "sound/audio_service.h"
#include "sound/sound_bank.h"

namespace ote {

// What a scene says about itself to the music (Scene::audioContext).
struct SceneAudio {
  enum Kind {
    Keep,    // leave whatever plays (options, error screens)
    Silent,  // the launcher: nothing
    Title,   // a game's page, its map list, its end card: the game's title music
    Scene,   // a campaign scene: scene music
    Battle   // a battle: map music, turn-phase music, story music, jingles
  };
  Kind kind = Keep;
  int map = -1;               // Battle: index into db.maps (-1 = unknown)
  bool storyAsked = false;    // Battle: the story had already asked for music (a resumed battle)
  std::string storyMusic;     // ... and for which track ("" = it stopped the music)
  bool enemyTurn = false;     // Battle: the side to move is not a human (a resumed battle)
  // Music by situation (docs/SOUND.md "Music by situation"), for a resumed battle: the side to move, the hostile units left, and the
  // overrides setPhaseMusic had set (side id -> music id, "" = no phase music).
  std::string phaseSide;
  int hostiles = -1;
  std::vector<std::pair<std::string, std::string>> overrides;
};

// A turn that starts: whose, whether a computer plays it, and how many units hostile to it are left (-1 = not counted).
struct PhaseInfo {
  std::string side;      // the side id ("" = not known: only the legacy player / enemy music applies)
  bool ai = false;
  int hostiles = -1;
};

class MusicDirector {
 public:
  static constexpr float kCrossFade = 0.5f;  // seconds

  void bind(AudioService* audio, const AudioData* data, const SoundBank* bank, const std::string& dir);
  // The scene on screen changed: pick the music for it (a no-op for Keep).
  void enter(const SceneAudio& scene);
  // A turn started in the battle: `enemy` = a side that is not a human on this device.
  void turnPhase(bool enemy);
  void turnPhase(const PhaseInfo& phase);
  // setPhaseMusic ran: `id` = the music ("" = no phase music for that side), `clear` = the override is taken away.
  void phaseOverride(const std::string& side, const std::string& id, bool clear);
  // The preparation screen is open / closed (its music: the map's, else the game's; none = the battle's own music goes on).
  void prep(bool open);
  // A shop screen is open / closed. `id` = the music of that shop ("" = the game's shopMusic, if it has one; else nothing changes).
  void shop(bool open, const std::string& id);
  // The story asked (playMusic / stopMusic; `id` empty = stop). fadeMs < 0 = the default cross-fade.
  void story(const std::string& id, int fadeMs);
  // The battle is decided: `victory` / `defeat` jingle, no music under it (it ends in silence); the level-up
  // jingle pauses the track and lets it go on afterwards.
  enum class Jingle { Victory, Defeat, LevelUp, Promotion, ChapterCard };
  void jingle(Jingle which);
  // Stops everything and forgets (the game is unloaded).
  void stop();

  int playing() const { return playing_; }  // track index, -1 = none (tests)

 private:
  int wanted(bool& keep) const;       // the track the current scene wants; keep = "do not change"
  void settle(int fadeMs = -1);       // make `playing_` what is wanted
  void play(int track, int fadeMs);   // start `track` (-1 = stop) unless it is the one playing

  AudioService* audio_ = nullptr;
  const AudioData* data_ = nullptr;
  const SoundBank* bank_ = nullptr;
  std::string dir_;

  SceneAudio::Kind kind_ = SceneAudio::Silent;
  int map_ = -1;
  bool enemy_ = false;          // the turn phase
  std::string side_;            // ... of this side ("" = unknown)
  bool few_ = false;            // the hostile units left were few when this phase started (decided at the start, not mid-phase)
  bool prep_ = false;           // the preparation screen is open
  bool shop_ = false;           // a shop screen is open (preparation or map)
  int shopTrack_ = -1;          // ... and the music its shop names (-1 = the game's shopMusic)
  std::vector<std::pair<std::string, int>> overrides_;  // setPhaseMusic: side id -> track (kSilent = "")
  int fewTrack() const;         // the few-enemies track of this map / game (-1 = none)
  int phaseTrack() const;       // the phase music of the side to move (-1 = none)
  int fewCount() const;
  bool storyAsked_ = false;
  int storyTrack_ = -1;         // -1 with storyAsked_ = stopped
  bool locked_ = false;         // a victory / defeat jingle is playing out: nothing else starts until the scene changes
  int playing_ = -1;            // the track asked of the service (a paused one under a level-up jingle stays here)
};

}  // namespace ote
