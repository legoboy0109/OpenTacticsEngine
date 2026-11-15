// The sound self-test: a short script of music and effect requests that --audio-test plays through
// the real device and --audio-render writes to a WAV file. One script for both, so what the owner
// hears is what the tests measured.
//
//   t=0    music: a synthesized melody (or --audio-file's file), looping
//   t=1.0  chime        t=2.0  chime
//   t=3.0  music stops with a 0.5 s fade
//   done at t=3.8
#pragma once
#include <string>

#include "sound/audio_service.h"

namespace ote {

class AudioSelfTest {
 public:
  static constexpr double kLength = 3.8;

  // musicFile empty = the built-in melody.
  AudioSelfTest(AudioService& audio, const std::string& musicFile);
  // Fires the requests that are due at the service's clock; call before AudioService::update.
  void update();
  bool finished() const { return audio_.clock() + 1e-6 >= kLength; }
  // Set when --audio-file could not be opened (the melody plays instead).
  const std::string& problem() const { return problem_; }

 private:
  AudioService& audio_;
  std::string musicFile_, problem_;
  SoundId chime_ = 0;
  int step_ = 0;
};

}  // namespace ote
