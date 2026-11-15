#include "sound/audio_service.h"

#include <cmath>

#include "audio/decode.h"
#include "core/base/log.h"
#include "sound/audio_files.h"

namespace ote {

AudioService::AudioService() : device_(createNullAudioOut(kRate)), mixer_(new Mixer(kRate)) {
  scratch_.assign(1024 * 2, 0);
  tick_ = addSound("tick", makeTick());
}

AudioService::~AudioService() {}

void AudioService::attachDevice(std::unique_ptr<AudioOut> device) {
  pending_ = false;
  if (!device || !device->available()) {
    if (mode_ == Mode::Live) mode_ = Mode::Off;
    return;
  }
  // The mixer makes exactly the rate the device was opened with (the backend converts for the hardware).
  if (device->rate() != mixer_->rate()) {
    int music = mixer_->musicLevel(), sound = mixer_->soundLevel();
    mixer_.reset(new Mixer(device->rate()));
    mixer_->setVolumes(music, sound);
  }
  OTE_INFO("audio: %s, %d Hz", device->name(), device->rate());
  device_ = std::move(device);
  mode_ = Mode::Live;
}

void AudioService::shutdown() {
  device_ = createNullAudioOut(kRate);
  pending_ = false;
  mode_ = Mode::Off;
  log_.close();
}

void AudioService::deviceFailed() {
  pending_ = false;
  mode_ = Mode::Off;
}

// ---- volumes

static std::string fmt(const char* f, double a) {
  char b[64];
  std::snprintf(b, sizeof b, f, a);
  return b;
}

void AudioService::setVolumes(int music, int sound) {
  int oldMusic = mixer_->musicLevel(), oldSound = mixer_->soundLevel();
  mixer_->setVolumes(music, sound);  // clamps to 0..10
  music = mixer_->musicLevel();
  sound = mixer_->soundLevel();
  if (!volumesLogged_ || music != oldMusic || sound != oldSound)
    log_.write(clock_, "volumes music=" + std::to_string(music) + " sound=" + std::to_string(sound));
  volumesLogged_ = true;
  // The music was asked for while its level was 0 (nothing was opened): now it may start.
  if (oldMusic == 0 && music > 0 && mode_ != Mode::Off && remembered_.valid && !mixer_->musicPlaying()) {
    MusicParams p = remembered_.params;
    p.fadeSeconds = 0.3f;
    std::string err;
    if (!startMusicFile(remembered_.path, p, err)) OTE_WARN("audio: %s", err.c_str());
  }
}

// ---- effects

SoundId AudioService::addSound(const std::string& name, std::shared_ptr<const PcmBuffer> buffer) {
  Sound s;
  s.name = name;
  s.buffer = buffer;  // made in code: tiny, always kept
  sounds_.push_back(s);
  return (SoundId)sounds_.size();
}

SoundId AudioService::loadSound(const std::string& path, std::string& err) {
  auto it = byPath_.find(path);
  if (it != byPath_.end()) return it->second;
  Sound s;
  s.name = path;
  if (mode_ != Mode::Off) {
    FileBytes bytes = readAudioFile(path, err);
    if (!bytes) return 0;
    PcmBuffer pcm;
    std::string why;
    if (!decodeSound(bytes->data(), bytes->size(), pcm, why)) {
      err = path + ": " + why;
      return 0;
    }
    if (mode_ == Mode::Live) s.buffer.reset(new PcmBuffer(std::move(pcm)));  // Silent mode only checked it
  }
  sounds_.push_back(s);
  SoundId id = (SoundId)sounds_.size();
  byPath_[path] = id;
  return id;
}

void AudioService::playSound(SoundId id, float volume) {
  if (id < 1 || id > (SoundId)sounds_.size()) return;
  const Sound& s = sounds_[(size_t)id - 1];
  log_.write(clock_, "sound " + s.name + " vol=" + fmt("%.0f", volume * 100.0) +
                         " level=" + std::to_string(mixer_->soundLevel()));
  if (device_->available() && s.buffer) mixer_->playSound(s.buffer, volume);
}

void AudioService::playTick(bool musicSlider) {
  const Sound& s = sounds_[(size_t)tick_ - 1];
  int level = musicSlider ? mixer_->musicLevel() : mixer_->soundLevel();
  log_.write(clock_, "sound " + s.name + " vol=100 bus=" + (musicSlider ? "music" : "sound") +
                         " level=" + std::to_string(level));
  if (device_->available() && s.buffer) mixer_->playSound(s.buffer, 1.0f, musicSlider);
}

// ---- music

static std::string musicPlayLine(const std::string& name, const MusicParams& p, int level) {
  return "music play " + name + " fade=" + fmt("%g", p.fadeSeconds) + " loop=" + (p.loop ? "1" : "0") +
         " level=" + std::to_string(level);
}

bool AudioService::playMusicFile(const std::string& path, const MusicParams& params, std::string& err) {
  if (mode_ == Mode::Off || mixer_->musicLevel() == 0) {
    // Nothing is opened: no file is read at level 0, and Off checks nothing.
    log_.write(clock_, musicPlayLine(path, params, mixer_->musicLevel()));
    remembered_.valid = true;
    remembered_.path = path;
    remembered_.params = params;
    return true;
  }
  return startMusicFile(path, params, err);
}

bool AudioService::startMusicFile(const std::string& path, const MusicParams& params, std::string& err) {
  FileBytes bytes = readAudioFile(path, err);
  if (!bytes) return false;
  std::string why;
  std::unique_ptr<MusicSource> src = openMusic(bytes, why);
  if (!src) {
    err = path + ": " + why;
    return false;
  }
  log_.write(clock_, musicPlayLine(path, params, mixer_->musicLevel()));
  remembered_.valid = true;
  remembered_.path = path;
  remembered_.params = params;
  if (mode_ == Mode::Live) mixer_->playMusic(std::move(src), params);  // Silent mode only checked the file
  return true;
}

void AudioService::playMusic(const std::string& name, std::unique_ptr<MusicSource> source, const MusicParams& params) {
  log_.write(clock_, musicPlayLine(name, params, mixer_->musicLevel()));
  remembered_.valid = false;  // cannot be reopened by name
  if (mode_ == Mode::Live && source) mixer_->playMusic(std::move(source), params);
}

void AudioService::stopMusic(float fadeSeconds) {
  log_.write(clock_, "music stop fade=" + fmt("%g", fadeSeconds));
  remembered_.valid = false;
  mixer_->stopMusic(fadeSeconds);
}

// ---- time and the device

void AudioService::pump() {
  if (!device_->available()) return;
  const int rate = mixer_->rate();
  int need = rate * kQueueMs / 1000 - device_->queuedFrames();
  if (need <= 0) return;
  int cap = rate * kCatchUpMs / 1000;  // after a long frame: refill, but do not mix seconds at once
  if (need > cap) need = cap;
  const int chunk = (int)scratch_.size() / 2;
  while (need > 0) {
    int n = need < chunk ? need : chunk;
    mixer_->mix(scratch_.data(), n);
    device_->push(scratch_.data(), n);
    need -= n;
  }
}

void AudioService::update(double dt) {
  clock_ += dt;
  pump();
}

}  // namespace ote
