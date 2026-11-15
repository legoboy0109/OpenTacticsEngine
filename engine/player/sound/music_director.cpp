#include "sound/music_director.h"

#include "core/base/log.h"

namespace ote {

void MusicDirector::bind(AudioService* audio, const AudioData* data, const SoundBank* bank, const std::string& dir) {
  audio_ = audio;
  data_ = data;
  bank_ = bank;
  dir_ = dir;
}

// The track the scene on screen wants. keep = it wants nothing in particular (leave what plays).
int MusicDirector::wanted(bool& keep) const {
  keep = false;
  if (!data_) return -1;
  switch (kind_) {
    case SceneAudio::Keep: keep = true; return -1;
    case SceneAudio::Silent: return -1;
    case SceneAudio::Title: return data_->titleMusic;
    case SceneAudio::Scene:
      if (storyAsked_) return storyTrack_;
      if (data_->sceneMusic >= 0) return data_->sceneMusic;
      keep = true;  // no scene music in the game: whatever played goes on
      return -1;
    case SceneAudio::Battle: {
      if (storyAsked_) return storyTrack_;
      // The turn phase's music: the map's own first, then the game's; "" on the map = no phase music there.
      const MapMusic mm = map_ >= 0 && (size_t)map_ < data_->maps.size() ? data_->maps[(size_t)map_] : MapMusic();
      const int phaseMap = enemy_ ? mm.enemyTurn : mm.playerTurn;
      const int phase = phaseMap != kNone ? phaseMap : enemy_ ? data_->enemyTurnMusic : data_->playerTurnMusic;
      if (phase >= 0) return phase;
      return mm.music != kNone ? (mm.music >= 0 ? mm.music : -1) : data_->mapMusic;
    }
  }
  return -1;
}

void MusicDirector::settle(int fadeMs) {
  if (locked_) return;
  bool keep = false;
  const int track = wanted(keep);
  if (!keep) play(track, fadeMs);
}

void MusicDirector::play(int track, int fadeMs) {
  if (!audio_ || !data_) return;
  if (track == playing_) return;  // the same track asked again keeps playing
  const float fade = fadeMs < 0 ? kCrossFade : (float)fadeMs / 1000.0f;
  playing_ = track;
  if (track < 0) {
    audio_->stopMusic(fade);
    return;
  }
  const MusicDef& m = data_->music[(size_t)track];
  if (bank_ && !bank_->musicFileOk(track)) {  // a missing file was warned about when the game loaded: silence
    audio_->stopMusic(fade);
    return;
  }
  MusicParams p;
  p.fadeSeconds = fade;
  p.loop = m.loop;
  p.loopStartSeconds = m.loopStart;
  p.volume = (float)m.volume / 100.0f;
  std::string err;
  if (!audio_->playMusicFile(dir_ + "/" + m.file, p, err)) {
    OTE_WARN("audio: music '%s': %s; silence instead", m.id.c_str(), err.c_str());
    audio_->stopMusic(fade);
  }
}

void MusicDirector::enter(const SceneAudio& scene) {
  if (scene.kind == SceneAudio::Keep) return;
  kind_ = scene.kind;
  map_ = scene.map;
  enemy_ = scene.kind == SceneAudio::Battle && scene.enemyTurn;
  locked_ = false;
  storyAsked_ = false;
  storyTrack_ = -1;
  if (scene.storyAsked && data_) {
    storyAsked_ = true;
    storyTrack_ = scene.storyMusic.empty() ? -1 : data_->findMusic(scene.storyMusic);
    if (!scene.storyMusic.empty() && storyTrack_ < 0)
      OTE_WARN("audio: the story asked for unknown music '%s'", scene.storyMusic.c_str());
  }
  settle();
}

void MusicDirector::turnPhase(bool enemy) {
  if (kind_ != SceneAudio::Battle || enemy == enemy_) return;
  enemy_ = enemy;
  settle();
}

void MusicDirector::story(const std::string& id, int fadeMs) {
  if (!data_ || (kind_ != SceneAudio::Battle && kind_ != SceneAudio::Scene)) return;
  int track = -1;
  if (!id.empty()) {
    track = data_->findMusic(id);
    if (track < 0) {  // the game loader refuses unknown ids; this is only reachable when audio.json was not loaded
      OTE_WARN("audio: the story asked for unknown music '%s'", id.c_str());
      return;
    }
  }
  storyAsked_ = true;
  storyTrack_ = track;
  settle(fadeMs);
}

void MusicDirector::jingle(Jingle which) {
  if (!audio_ || !data_) return;
  const int track = which == Jingle::Victory ? data_->victory : which == Jingle::Defeat ? data_->defeat : data_->levelUp;
  if (track < 0 || (bank_ && !bank_->musicFileOk(track))) return;
  const MusicDef& m = data_->music[(size_t)track];
  MusicParams p;
  p.loop = false;
  p.loopStartSeconds = 0;
  p.volume = (float)m.volume / 100.0f;
  if (which == Jingle::LevelUp) {
    p.pauseCurrent = true;  // the track under it resumes when the jingle ends; playing_ stays that track
    p.fadeSeconds = 0;
  } else {
    p.fadeSeconds = 0;      // the battle's music is cut: the jingle is the last thing heard
    locked_ = true;
    playing_ = track;
  }
  std::string err;
  if (!audio_->playMusicFile(dir_ + "/" + m.file, p, err)) OTE_WARN("audio: jingle '%s': %s", m.id.c_str(), err.c_str());
}

void MusicDirector::stop() {
  if (audio_ && playing_ >= 0) audio_->stopMusic(kCrossFade);
  playing_ = -1;
  kind_ = SceneAudio::Silent;
  map_ = -1;
  enemy_ = false;
  storyAsked_ = false;
  storyTrack_ = -1;
  locked_ = false;
}

}  // namespace ote
