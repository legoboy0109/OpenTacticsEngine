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
      if (shop_) {  // a shop screen: its own music, else the game's shopMusic, else whatever else would play
        const int s = shopTrack_ >= 0 ? shopTrack_ : data_->shopMusic;
        if (s >= 0) return s;
      }
      const MapMusic mm = map_ >= 0 && (size_t)map_ < data_->maps.size() ? data_->maps[(size_t)map_] : MapMusic();
      if (prep_) {  // the preparation screen: the map's, else the game's; none = the battle's own music goes on
        const int p = mm.prep != kNone ? mm.prep : data_->prepMusic;
        if (p >= 0) return p;
      }
      // The turn phase's music, most specific first: what setPhaseMusic set for this side, the few-enemies track, the map's /
      // game's music for this side, the map's / game's player or enemy turn music. "" anywhere = no phase music there.
      int phase = phaseTrack();
      if (phase >= 0) return phase;
      return mm.music != kNone ? (mm.music >= 0 ? mm.music : -1) : data_->mapMusic;
    }
  }
  return -1;
}

// The few-enemies track: the map's, else the game's (the map's "" turns it off there).
int MusicDirector::fewTrack() const {
  const MapMusic mm = map_ >= 0 && (size_t)map_ < data_->maps.size() ? data_->maps[(size_t)map_] : MapMusic();
  if (mm.few == kSilent) return -1;
  return mm.few != kNone ? mm.few : data_->fewMusic;
}

int MusicDirector::phaseTrack() const {
  const MapMusic mm = map_ >= 0 && (size_t)map_ < data_->maps.size() ? data_->maps[(size_t)map_] : MapMusic();
  for (const auto& o : overrides_)
    if (o.first == side_) return o.second >= 0 ? o.second : -1;  // setPhaseMusic: "" = no phase music, nothing below applies
  if (few_) {
    const int f = fewTrack();
    if (f >= 0) return f;
  }
  for (const auto& p : mm.phase)
    if (p.first == side_) return p.second >= 0 ? p.second : -1;
  for (const auto& p : data_->phaseMusic)
    if (p.first == side_) return p.second;
  const int phaseMap = enemy_ ? mm.enemyTurn : mm.playerTurn;
  const int phase = phaseMap != kNone ? phaseMap : enemy_ ? data_->enemyTurnMusic : data_->playerTurnMusic;
  return phase >= 0 ? phase : -1;
}

void MusicDirector::settle(int fadeMs) {
  if (locked_ || bootWaiting_) return;  // a victory jingle / the boot jingle plays out first
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
  side_ = scene.kind == SceneAudio::Battle ? scene.phaseSide : std::string();
  few_ = false;
  prep_ = false;
  bootWaiting_ = false;
  shop_ = false;
  shopTrack_ = -1;
  overrides_.clear();
  locked_ = false;
  storyAsked_ = false;
  storyTrack_ = -1;
  if (scene.storyAsked && data_) {
    storyAsked_ = true;
    storyTrack_ = scene.storyMusic.empty() ? -1 : data_->findMusic(scene.storyMusic);
    if (!scene.storyMusic.empty() && storyTrack_ < 0)
      OTE_WARN("audio: the story asked for unknown music '%s'", scene.storyMusic.c_str());
  }
  if (kind_ == SceneAudio::Battle && data_) {
    for (const auto& o : scene.overrides) {
      int t = o.second.empty() ? kSilent : data_->findMusic(o.second);
      if (t == -1) OTE_WARN("audio: the story asked for unknown phase music '%s'", o.second.c_str());
      else overrides_.push_back(std::make_pair(o.first, t));
    }
    if (!enemy_ && scene.hostiles >= 0) few_ = fewTrack() >= 0 && scene.hostiles <= fewCount();
  }
  if (kind_ == SceneAudio::Title && data_ && data_->bootJingle >= 0 && !bootDone_) startBootJingle();
  settle();
}

// The game's page opened for the first time since the game was loaded: its boot jingle plays alone, and titleMusic follows when it ends
// (tick). A missing file or a refused open simply skips the jingle.
void MusicDirector::startBootJingle() {
  bootDone_ = true;
  const int track = data_->bootJingle;
  if (!audio_ || (bank_ && !bank_->musicFileOk(track))) return;
  const MusicDef& m = data_->music[(size_t)track];
  MusicParams p;
  p.loop = false;
  p.loopStartSeconds = 0;
  p.volume = (float)m.volume / 100.0f;
  p.fadeSeconds = 0;
  std::string err;
  if (!audio_->playMusicFile(dir_ + "/" + m.file, p, err)) {
    OTE_WARN("audio: boot jingle '%s': %s", m.id.c_str(), err.c_str());
    return;
  }
  playing_ = -2;  // nothing of the scene's own is playing yet: the first settle after the jingle starts it
  bootWaiting_ = true;
}

// Once per frame: the boot jingle has ended (or was never heard): the scene's music starts.
void MusicDirector::tick() {
  if (!bootWaiting_ || !audio_ || audio_->musicPlaying()) return;
  bootWaiting_ = false;
  settle();
}

int MusicDirector::fewCount() const {
  const MapMusic mm = map_ >= 0 && (size_t)map_ < data_->maps.size() ? data_->maps[(size_t)map_] : MapMusic();
  return mm.few != kNone ? mm.fewCount : data_->fewCount;
}

void MusicDirector::turnPhase(bool enemy) {
  PhaseInfo p;
  p.ai = enemy;
  p.side = side_;
  p.hostiles = -1;
  turnPhase(p);
}

// A turn starts. The few-enemies decision is made here and stays for the whole phase (it never switches mid-phase).
void MusicDirector::turnPhase(const PhaseInfo& phase) {
  if (kind_ != SceneAudio::Battle || !data_) return;
  const bool few = !phase.ai && phase.hostiles >= 0 && fewTrack() >= 0 && phase.hostiles <= fewCount();
  if (phase.ai == enemy_ && phase.side == side_ && few == few_) return;
  enemy_ = phase.ai;
  side_ = phase.side;
  few_ = few;
  settle();
}

void MusicDirector::phaseOverride(const std::string& side, const std::string& id, bool clear) {
  if (kind_ != SceneAudio::Battle || !data_) return;
  for (size_t i = 0; i < overrides_.size(); ++i)
    if (overrides_[i].first == side) {
      overrides_.erase(overrides_.begin() + (long)i);
      break;
    }
  if (!clear) {
    const int t = id.empty() ? kSilent : data_->findMusic(id);
    if (t == -1) {  // the game loader refuses unknown ids; only reachable when audio.json was not loaded
      OTE_WARN("audio: the story asked for unknown phase music '%s'", id.c_str());
      return;
    }
    overrides_.push_back(std::make_pair(side, t));
  }
  if (side == side_) settle();  // it is that side's phase now: the music changes at once; otherwise at the side's next phase
}

void MusicDirector::prep(bool open) {
  if (kind_ != SceneAudio::Battle || open == prep_) return;
  prep_ = open;
  settle();
}

void MusicDirector::shop(bool open, const std::string& id) {
  if (kind_ != SceneAudio::Battle || !data_) return;
  const int track = open && !id.empty() ? data_->findMusic(id) : -1;
  if (open == shop_ && track == shopTrack_) return;
  shop_ = open;
  shopTrack_ = track;
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
  const int track = which == Jingle::Victory ? data_->victory : which == Jingle::Defeat ? data_->defeat
                    : which == Jingle::LevelUp ? data_->levelUp : which == Jingle::Promotion ? data_->promotion : data_->chapterCard;
  if (track < 0 || (bank_ && !bank_->musicFileOk(track))) return;
  const MusicDef& m = data_->music[(size_t)track];
  MusicParams p;
  p.loop = false;
  p.loopStartSeconds = 0;
  p.volume = (float)m.volume / 100.0f;
  if (which == Jingle::LevelUp || which == Jingle::Promotion || which == Jingle::ChapterCard) {
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
  side_.clear();
  few_ = prep_ = false;
  overrides_.clear();
  storyAsked_ = false;
  storyTrack_ = -1;
  locked_ = false;
  bootWaiting_ = false;
  bootDone_ = false;  // the next time a game's page opens it is a new start
}

}  // namespace ote
