// The player-facing sound service (docs/SOUND.md): load sounds, play effects and music, set the
// volumes, and once per frame mix what the device is missing. One per player, owned by PlayerApp
// and reached through SceneHost::audio().
//
// Three modes, because a machine without sound (or a test run) must behave exactly like one with:
//   Off     no device and no decoding: --no-audio, or the device could not be opened. Requests are
//           only logged.
//   Silent  headless runs: effects are decoded (so a damaged file is found by tests) and music files
//           are opened and checked, but nothing is mixed. Requests are logged.
//   Live    a real device (or one that is waiting for the first key press on the web): everything works.
// Nothing here blocks, allocates while mixing, or uses a thread.
#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "sound/audio_log.h"
#include "audio/mixer.h"
#include "audio/synth.h"
#include "platform/platform.h"

namespace ote {

typedef int SoundId;  // 0 = no sound

class AudioService {
 public:
  enum class Mode { Off, Silent, Live };
  static const int kRate = 44100;          // what the mixer makes and the device is opened with
  static const int kQueueMs = 80;          // sound kept queued in the device
  static const int kCatchUpMs = 250;       // the most one frame pushes after a long frame

  AudioService();
  ~AudioService();

  // ---- set-up (the player does this once, see PlayerApp::onStart)
  void setMode(Mode m) { mode_ = m; }
  Mode mode() const { return mode_; }
  // Gives the service its output device. A device that is not available() leaves it in Silent/Off.
  void attachDevice(std::unique_ptr<AudioOut> device);
  // The device could not be opened (after a warning): carry on without sound.
  void deviceFailed();
  // The web: Live, but the device waits for the first key press (devicePending() says so).
  void waitForUserInput() { pending_ = true; mode_ = Mode::Live; }
  bool devicePending() const { return pending_; }
  bool deviceReady() const { return device_->available(); }
  // Lets go of the device (the platform is about to shut down) and closes the log; the service
  // then behaves like Off. Safe to call twice.
  void shutdown();
  bool openLog(const std::string& path, std::string& err) { return log_.open(path, err); }
  AudioLog& log() { return log_; }

  // ---- volumes, 0..10 each (docs/SOUND.md "Options")
  void setVolumes(int music, int sound);
  int musicVolume() const { return mixer_->musicLevel(); }
  int soundVolume() const { return mixer_->soundLevel(); }

  // ---- effects
  // Reads and decodes a .wav / .ogg once (cached by path). 0 and a plain message in err when it is
  // damaged, unsupported or over the limits (10 s / 2 MB decoded). Off mode checks nothing and hands out an id.
  SoundId loadSound(const std::string& path, std::string& err);
  // An effect made in code (the options tick, the self-test chime); `name` is what the log shows.
  SoundId addSound(const std::string& name, std::shared_ptr<const PcmBuffer> buffer);
  // volume 0..1 on top of the sound level. Unknown ids do nothing.
  void playSound(SoundId id, float volume = 1.0f);
  // The click heard while a volume slider moves; follows the music level when `musicSlider`.
  void playTick(bool musicSlider);

  // ---- music
  // Streams a .wav / .ogg. False (and a warning, nothing changes) when it cannot be opened. At music
  // level 0 nothing is opened or decoded: the request is only logged and remembered, and starts when the
  // level is raised.
  bool playMusicFile(const std::string& path, const MusicParams& params, std::string& err);
  // Music made in code (the self-test melody). `name` is what the log shows.
  void playMusic(const std::string& name, std::unique_ptr<MusicSource> source, const MusicParams& params);
  void stopMusic(float fadeSeconds);
  bool musicPlaying() const { return mixer_->musicPlaying(); }

  // Once per frame: advances the clock and tops the device queue up to ~80 ms.
  void update(double dt);
  double clock() const { return clock_; }
  // Mixes into the device now (what update() does); public for tests with a fake device.
  void pump();
  Mixer& mixer() { return *mixer_; }

 private:
  struct Sound {
    std::string name;
    std::shared_ptr<const PcmBuffer> buffer;  // null in Off / Silent mode
  };
  bool wantsSamples() const { return mode_ == Mode::Live; }
  bool startMusicFile(const std::string& path, const MusicParams& params, std::string& err);

  Mode mode_ = Mode::Off;
  bool pending_ = false;
  std::unique_ptr<AudioOut> device_;
  std::unique_ptr<Mixer> mixer_;
  AudioLog log_;
  double clock_ = 0;
  bool volumesLogged_ = false;
  std::vector<Sound> sounds_;  // index = SoundId - 1
  std::map<std::string, SoundId> byPath_;
  SoundId tick_ = 0;
  // The last music asked for by file, kept while the music level is 0.
  struct Remembered {
    bool valid = false;
    std::string path;
    MusicParams params;
  } remembered_;
  std::vector<int16_t> scratch_;
};

}  // namespace ote
