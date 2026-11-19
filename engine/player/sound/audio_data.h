// A game's sound data (docs/SOUND.md): data/audio.json plus the sound overrides on weapons, units, items and maps.
// Presentation only, like graphics.json: the rules core never sees it (it only lets the keys pass), it is not part
// of the save hash, and a game without data/audio.json is silent.
//
// Loaded by the player in three steps, so the launcher's game page (no Database) can have its title music:
//   loadAudioFile        data/audio.json alone: tracks, sounds, the standard set. Strict: unknown keys and unknown
//                        ids are errors that name the file, the entry and the key.
//   loadAudioOverrides   needs the Database: "sounds" on weapons / units / items, "music" keys on map files.
//   checkStoryAudio      every id a story command or a scene names must exist.
// A missing audio FILE is not an error here (the bank warns and the game goes on silent, sound_bank.h).
#pragma once
#include <string>
#include <vector>

#include "core/data/database.h"
#include "sound/cues.h"

namespace ote {

// Longest a cue's sound list can be, and the limits of the numbers (loader refusals).
constexpr int kMaxSoundVariants = 16;
constexpr float kMaxLoopStart = 36000.0f;  // seconds

struct MusicDef {
  std::string id, file;   // file: inside the game folder, "audio/title.ogg"
  float loopStart = 0;    // seconds; where a looping track jumps back to
  int volume = 100;       // percent of the music level
  bool loop = true;       // false: a jingle
};

struct SoundDef {
  std::string id;
  std::vector<std::string> files;  // one is picked each time, in turn (SoundBank)
  int volume = 100;                // percent
};

// What each cue plays: an index into AudioData::sounds, kNone = not set here (the next layer of the lookup
// decides), kSilent = set to "" on purpose (the lookup stops and nothing plays).
constexpr int kNone = -1, kSilent = -2;
struct CueSet {
  int sound[kCueCount];
  int healScene = kNone;  // an item's "healScene": its heal sound in the battle scene only (kNone = the "heal" one is used there too)
  CueSet() { for (int& s : sound) s = kNone; }
};

// The music a map file asks for: indices into AudioData::music, kNone / kSilent as above.
struct MapMusic {
  int music = kNone, playerTurn = kNone, enemyTurn = kNone;
  // Music by situation (docs/SOUND.md "Music by situation"): a phase's music per side id ("" = none on this map: kSilent),
  // the few-enemies track and the count it applies at (few = kNone: the game's), and the preparation screen's music.
  std::vector<std::pair<std::string, int>> phase;
  int few = kNone, fewCount = 0, prep = kNone;
};

struct AudioData {
  bool present = false;  // data/audio.json exists
  std::vector<MusicDef> music;
  std::vector<SoundDef> sounds;
  // `use`: indices into `music`, -1 = none
  int titleMusic = -1, mapMusic = -1, playerTurnMusic = -1, enemyTurnMusic = -1, sceneMusic = -1;
  int victory = -1, defeat = -1, levelUp = -1;
  // Music by situation: a phase's music per side id, the few-enemies track (while the hostile units left are at most fewCount
  // at the start of a human side's phase), the preparation screen's music, and two more jingles.
  std::vector<std::pair<std::string, int>> phaseMusic;
  int fewMusic = -1, fewCount = 0;
  int prepMusic = -1, promotion = -1, chapterCard = -1;
  int bootJingle = -1;  // "bootJingle": played once when a game's page opens, before titleMusic (docs/SOUND.md "Boot jingle")
  std::vector<std::pair<int, int>> promotionSounds;  // "promotionSounds": (ms after the promotion starts, sound index), in time order
  int shopMusic = -1;  // "shopMusic": the music of any shop screen whose shop names none of its own (docs/SOUND.md "Shop music")
  CueSet standard;
  // Overrides, by index into db.weapons / db.unitTypes / db.items / db.maps (empty until loadAudioOverrides).
  std::vector<CueSet> weapons, units, items;
  // Step sounds by movement type (docs/SOUND.md): per db.moveTypes index. And the seconds between two steps (0 = the default,
  // 0.10) per unit type and per movement type (a unit type's own wins).
  std::vector<CueSet> moveTypes;
  std::vector<float> unitStepGap, moveStepGap;
  std::vector<MapMusic> maps;
  bool hasOverrides = false;

  int findMusic(const std::string& id) const;
  int findSound(const std::string& id) const;
  // Nothing in the game makes any sound or music.
  bool silent() const { return !present; }
};

// `dir` is the game folder. A missing data/audio.json is fine (present stays false). Messages start with the
// file's path: "<dir>/data/audio.json: sounds \"hit\": unknown key \"flie\" (expected ...)".
bool loadAudioFile(const std::string& dir, AudioData& out, std::string& err);
bool loadAudioOverrides(const Database& db, AudioData& io, std::string& err);
// Every playMusic / playSound id of the maps and of data/campaign.json names a track / sound of audio.json
// (a game without audio.json that uses these commands is refused: they would never be heard).
bool checkStoryAudio(const Database& db, const AudioData& data, std::string& err);

}  // namespace ote
