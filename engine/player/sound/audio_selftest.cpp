#include "sound/audio_selftest.h"

#include "core/base/log.h"

namespace ote {

AudioSelfTest::AudioSelfTest(AudioService& audio, const std::string& musicFile)
    : audio_(audio), musicFile_(musicFile), chime_(audio.addSound("chime", makeChime())) {}

void AudioSelfTest::update() {
  const double t = audio_.clock() + 1e-6;  // sums of 1/60 s land a hair below the exact second
  if (step_ == 0) {
    step_ = 1;
    MusicParams p;
    bool played = false;
    if (!musicFile_.empty()) {
      played = audio_.playMusicFile(musicFile_, p, problem_);
      if (!played) OTE_WARN("audio test: %s; playing the built-in melody", problem_.c_str());
    }
    if (!played) audio_.playMusic("melody", makeBufferSource(makeMelody()), p);
  }
  if (step_ == 1 && t >= 1.0) { step_ = 2; audio_.playSound(chime_); }
  if (step_ == 2 && t >= 2.0) { step_ = 3; audio_.playSound(chime_); }
  if (step_ == 3 && t >= 3.0) { step_ = 4; audio_.stopMusic(0.5f); }
}

}  // namespace ote
