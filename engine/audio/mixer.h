// The software mixer: 16 effect voices + 2 music voices -> 16-bit stereo at
// one fixed output rate. A pure function of what it was told: mix(out, n)
// produces the next n frames and nothing else happens (no clock, no threads,
// no allocation while mixing), so tests mix into memory and compare samples.
// Fades are counted in output frames, so they are exact whatever the caller's
// timing. See docs/SOUND.md "Engine design".
#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "audio/music_source.h"
#include "audio/pcm.h"

namespace ote {

constexpr int kSoundVoices = 16;
constexpr int kMusicVoices = 2;
constexpr int kMusicBlockFrames = 2048;  // source frames a music voice decodes ahead

// Volume level 0..10 -> gain 0..1 on a perceptual (squared) curve: 10 = 1.0, 5 = 0.25, 0 = silent.
float volumeGain(int level);

struct MusicParams {
  float fadeSeconds = 0;       // fade in of this music, and fade out of what it replaces (cross-fade)
  bool loop = true;
  double loopStartSeconds = 0; // where a loop jumps back to (seconds into the source)
  float volume = 1;            // 0..1, this track's own level (the "volume" percent of audio.json)
  // Pause (do not stop) the music playing now and resume it when this one ends: for jingles.
  // A jingle must not loop.
  bool pauseCurrent = false;
};

class Mixer {
 public:
  explicit Mixer(int outRate);

  int rate() const { return rate_; }
  // Master levels 0..10 (clamped). Sound level 0 plays no effects; music level 0 freezes the
  // music voices without decoding anything, so it costs nothing and picks up where it was.
  void setVolumes(int musicLevel, int soundLevel);
  int musicLevel() const { return musicLevel_; }
  int soundLevel() const { return soundLevel_; }

  // ---- effects
  // Starts an effect; volume 0..1. onMusicBus: the effect follows the music level instead of the
  // sound level (the options screen's tick while the music slider moves). When all 16 voices are
  // busy the oldest one is cut. Returns the voice index, or -1 when nothing was started.
  int playSound(const std::shared_ptr<const PcmBuffer>& buffer, float volume = 1, bool onMusicBus = false);
  void stopSounds();
  int activeSounds() const;

  // ---- music
  // Starts `source` (the mixer owns it from now on). Without pauseCurrent whatever plays fades out
  // over params.fadeSeconds while the new music fades in (equal power, so the loudness stays level).
  void playMusic(std::unique_ptr<MusicSource> source, const MusicParams& params);
  void stopMusic(float fadeSeconds);  // fades out whatever plays (and drops anything paused)
  void pauseMusic();                  // the current music holds still (a short ramp avoids a click)
  void resumeMusic();
  bool musicPlaying() const;          // some music is audible or fading in (paused counts as not playing)
  bool musicPaused() const;
  int musicVoicesInUse() const;       // 0..2, for tests

  // Writes the next `frames` frames (interleaved L R, overwritten) of everything playing.
  void mix(int16_t* out, int frames);

 private:
  struct SoundVoice {
    std::shared_ptr<const PcmBuffer> buffer;
    double pos = 0, step = 1;
    float volume = 1;
    bool onMusicBus = false;
    bool active = false;
    uint64_t seq = 0;  // start order: the lowest active one is the oldest
  };
  struct MusicVoice {
    std::unique_ptr<MusicSource> source;
    bool active = false;
    bool loop = true;
    int64_t loopStartFrame = 0;
    float volume = 1;
    int channels = 1;
    double step = 1;
    std::vector<int16_t> block;  // decoded source frames, allocated once
    int have = 0;                // frames in block
    double pos = 0;              // read position in block, in source frames
    bool eof = false;
    // Fade: `level` (0..1) moves by `inc` per output frame towards `target`; the gain is
    // sin(level * pi/2), so a voice fading in while another fades out stays at constant power.
    float level = 1, target = 1, inc = 0;
    bool paused = false;
    bool pausedForJingle = false;  // resumes by itself when the jingle ends
    float pauseGain = 1;           // ramps to 0 when paused
  };

  void mixChunk(int16_t* out, int n);
  void mixSounds(float* acc, int n);
  void mixMusic(float* acc, int n);
  void mixMusicVoice(MusicVoice& v, float* acc, int n);
  bool ensureFrames(MusicVoice& v);  // false when the music has ended
  void refill(MusicVoice& v);
  void startFade(MusicVoice& v, float target, float seconds);
  void killMusicVoice(MusicVoice& v);
  void onMusicEnded(int voice);
  int freeMusicVoice();

  int rate_;
  int musicLevel_ = 7, soundLevel_ = 7;
  float musicGain_, soundGain_;
  SoundVoice sounds_[kSoundVoices];
  MusicVoice music_[kMusicVoices];
  int current_ = -1;  // the music voice that is "the" music (not fading out), or -1
  uint64_t nextSeq_ = 1;
  std::vector<float> acc_;  // one chunk of stereo floats
};

}  // namespace ote
