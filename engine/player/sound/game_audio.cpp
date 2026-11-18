#include "sound/game_audio.h"

#include "core/base/log.h"

namespace ote {
namespace {

// Seconds between two plays of the same cue: a walk has a step per square and a line of text a blip per letter,
// far more than an ear wants.
double minGap(Cue c) {
  switch (c) {
    case Cue::Step: return 0.10;
    case Cue::Dialogue: return 0.045;
    case Cue::Cursor: return 0.03;
    default: return 0;
  }
}

}  // namespace

bool GameAudio::load(const std::string& gameDir, const std::string& gameId, const Database* db, std::string& err) {
  if (audio_.mode() == AudioService::Mode::Off) {  // nothing is read, decoded or checked: a silent machine plays the same game
    if (gameId != gameId_) unload();
    gameId_ = gameId;
    dir_ = gameDir;
    return true;
  }
  if (gameId != gameId_) unload();
  AudioData fresh;
  if (!loadAudioFile(gameDir, fresh, err)) return false;
  if (db && fresh.present && (!loadAudioOverrides(*db, fresh, err) || !checkStoryAudio(*db, fresh, err))) return false;
  if (db && !fresh.present && !checkStoryAudio(*db, fresh, err)) return false;
  SoundBank bank;
  if (fresh.present && !bank.load(audio_, gameDir, fresh, err)) return false;
  gameId_ = gameId;
  dir_ = gameDir;
  data_ = fresh;
  bank_ = bank;
  full_ = db != nullptr;
  soundFrame_.assign(data_.sounds.size(), -1);
  for (bool& p : played_) p = false;
  director_.bind(&audio_, &data_, &bank_, gameDir);
  if (data_.present) OTE_INFO("audio: '%s': %d tracks, %d sounds", gameId.c_str(), (int)data_.music.size(), (int)data_.sounds.size());
  return true;
}

void GameAudio::unload() {
  director_.stop();
  director_.bind(nullptr, nullptr, nullptr, "");
  data_ = AudioData();
  bank_.clear();
  gameId_.clear();
  dir_.clear();
  full_ = false;
  soundFrame_.clear();
}

void GameAudio::beginFrame() { ++frame_; }

void GameAudio::playSoundIndex(int index, int cueSlot, double gap) {
  if (index < 0 || (size_t)index >= data_.sounds.size()) return;
  if (soundFrame_[(size_t)index] == frame_) return;  // the same sound twice in one frame is one sound
  const double now = audio_.clock();
  const int c = cueSlot;
  if (c >= 0 && played_[c] && now - lastCue_[c] < (gap >= 0 ? gap : minGap((Cue)c))) return;
  SoundId id = bank_.pick(index);
  if (id == 0) return;  // its file is missing: silence
  soundFrame_[(size_t)index] = frame_;
  if (c >= 0) {
    lastCue_[c] = now;
    played_[c] = true;
  }
  audio_.playSound(id, (float)data_.sounds[(size_t)index].volume / 100.0f);
}

void GameAudio::cue(Cue cue, const CueCtx& ctx) {
  if (!data_.present) return;
  int s = SoundBank::lookup(data_, cue, ctx);
  if (s < 0 && cue == Cue::Select) {  // a game without a select sound hears its confirm
    s = SoundBank::lookup(data_, Cue::Confirm, CueCtx());
    cue = Cue::Confirm;
  }
  double gap = -1;
  if (cue == Cue::Step) {  // seconds between two steps: the unit type's own, else its movement type's, else the default (docs/SOUND.md)
    const float u = ctx.unitType >= 0 && (size_t)ctx.unitType < data_.unitStepGap.size() ? data_.unitStepGap[(size_t)ctx.unitType] : 0.0f;
    const float m = ctx.moveType >= 0 && (size_t)ctx.moveType < data_.moveStepGap.size() ? data_.moveStepGap[(size_t)ctx.moveType] : 0.0f;
    gap = u > 0 ? u : m > 0 ? m : -1;
  }
  playSoundIndex(s, (int)cue, gap);
}

void GameAudio::sound(const std::string& id) {
  if (!data_.present) return;
  const int s = data_.findSound(id);
  if (s < 0) {
    OTE_WARN("audio: the story asked for unknown sound '%s'", id.c_str());
    return;
  }
  playSoundIndex(s, -1);
}

}  // namespace ote
