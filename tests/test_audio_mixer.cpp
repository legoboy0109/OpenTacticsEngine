// The mixer: exact samples for simple cases, resampling, voices, music loops, cross-fades.
#include <algorithm>
#include <cmath>

#include "audio_fixture.h"
#include "test.h"

using namespace ote;
using namespace audio_test;

namespace {
const double kPi = 3.14159265358979323846;

Mixer fullVolume(int rate = 44100) {
  Mixer m(rate);
  m.setVolumes(10, 10);  // gain 1.0: samples come out exactly as they went in
  return m;
}
}  // namespace

TEST(audio_mixer_volume_curve) {
  CHECK(volumeGain(0) == 0.0f);
  CHECK(volumeGain(10) == 1.0f);
  CHECK(std::fabs(volumeGain(5) - 0.25f) < 1e-6);
  CHECK(volumeGain(-3) == 0.0f && volumeGain(99) == 1.0f);
  for (int i = 1; i <= 10; ++i) CHECK(volumeGain(i) > volumeGain(i - 1));
}

TEST(audio_mixer_silence_and_exact_samples) {
  Mixer m = fullVolume();
  std::vector<int16_t> s = mixFrames(m, 200);
  for (int16_t v : s) CHECK_EQ(v, (int16_t)0);
  // Mono goes to both channels; the same rate copies the samples exactly.
  m.playSound(makeBuffer(44100, 1, 100, [](int i, int) { return i * 100 - 3000; }));
  s = mixFrames(m, 150);
  for (int i = 0; i < 100; ++i) {
    CHECK_EQ(s[2 * i], (int16_t)(i * 100 - 3000));
    CHECK_EQ(s[2 * i + 1], (int16_t)(i * 100 - 3000));
  }
  CHECK_EQ(s[200], (int16_t)0);
  CHECK_EQ(m.activeSounds(), 0);  // it ended and freed its voice
  // Stereo keeps the channels apart.
  m.playSound(makeBuffer(44100, 2, 10, [](int i, int c) { return c ? -i : i; }));
  s = mixFrames(m, 10);
  CHECK_EQ(s[6], (int16_t)3);
  CHECK_EQ(s[7], (int16_t)-3);
}

TEST(audio_mixer_voice_volume_and_master_levels) {
  Mixer m = fullVolume();
  std::shared_ptr<PcmBuffer> b = makeBuffer(44100, 1, 64, [](int, int) { return 10000; });
  m.playSound(b, 0.5f);
  CHECK_EQ(mixFrames(m, 1)[0], (int16_t)5000);
  m.setVolumes(10, 5);  // sound level 5 = gain 0.25
  m.playSound(b, 1.0f);
  CHECK_EQ(mixFrames(m, 1)[0], (int16_t)(1250 + 2500));  // the first one at 0.5 x 0.25, the new one at 0.25
  m.stopSounds();
  m.setVolumes(10, 0);  // sound level 0: nothing plays
  CHECK_EQ(m.playSound(b), -1);
  CHECK_EQ(m.activeSounds(), 0);
  // The tick on the music bus follows the music level instead.
  m.setVolumes(5, 0);
  CHECK(m.playSound(b, 1.0f, true) >= 0);
  CHECK_EQ(mixFrames(m, 1)[0], (int16_t)2500);
}

TEST(audio_mixer_clips_by_saturation) {
  Mixer m = fullVolume();
  std::shared_ptr<PcmBuffer> hi = makeBuffer(44100, 1, 10, [](int, int) { return 30000; });
  std::shared_ptr<PcmBuffer> lo = makeBuffer(44100, 1, 10, [](int, int) { return -30000; });
  m.playSound(hi);
  m.playSound(hi);
  CHECK_EQ(mixFrames(m, 1)[0], (int16_t)32767);
  m.stopSounds();
  m.playSound(lo);
  m.playSound(lo);
  CHECK_EQ(mixFrames(m, 1)[0], (int16_t)-32768);
}

TEST(audio_mixer_steals_the_oldest_effect) {
  Mixer m = fullVolume();
  // Sixteen long effects of different levels (1, 2, 4, ...) started in order, the first being the oldest.
  std::vector<std::shared_ptr<PcmBuffer>> bufs;
  for (int v = 0; v < kSoundVoices; ++v) bufs.push_back(makeBuffer(44100, 1, 1000, [v](int, int) { return 1 << (v % 12); }));
  for (int v = 0; v < kSoundVoices; ++v) m.playSound(bufs[v] , 1.0f);
  CHECK_EQ(m.activeSounds(), kSoundVoices);
  int expected = 0;
  for (int v = 0; v < kSoundVoices; ++v) expected += 1 << (v % 12);
  CHECK_EQ(mixFrames(m, 1)[0], (int16_t)std::min(expected, 32767));
  // A 17th: the oldest (value 1) is cut, still 16 voices, and the new one (value 4096... use 3) is heard.
  std::shared_ptr<PcmBuffer> extra = makeBuffer(44100, 1, 1000, [](int, int) { return 3; });
  m.playSound(extra, 1.0f);
  CHECK_EQ(m.activeSounds(), kSoundVoices);
  CHECK_EQ(mixFrames(m, 1)[0], (int16_t)std::min(expected - 1 + 3, 32767));
  // And again: now the second oldest (value 2) goes.
  m.playSound(extra, 1.0f);
  CHECK_EQ(mixFrames(m, 1)[0], (int16_t)std::min(expected - 1 - 2 + 6, 32767));
}

TEST(audio_mixer_resamples_22050_to_44100_with_linear_interpolation) {
  Mixer m = fullVolume(44100);
  std::shared_ptr<PcmBuffer> tone = makeTone(440, 1.0, 22050, 1, 0.5);
  m.playSound(tone);
  std::vector<int16_t> s = mixFrames(m, 44100);
  // 440 Hz stays 440 Hz: one upward crossing per period.
  int crossings = upCrossings(s, 0, 44100);
  CHECK(crossings >= 439 && crossings <= 441);
  // Every other output frame is a source frame, in between is the average of its neighbours.
  for (int i = 0; i < 2000; ++i) {
    CHECK_EQ(s[(size_t)(2 * i) * 2], tone->samples[(size_t)i]);
    int expect = (int)std::lrint(0.5 * (tone->samples[(size_t)i] + tone->samples[(size_t)i + 1]));
    CHECK(std::abs(s[(size_t)(2 * i + 1) * 2] - expect) <= 1);
  }
  // Against the ideal 44.1 kHz sine: linear interpolation of 50 samples per period is within 1%.
  double worst = 0;
  for (int i = 0; i < 44000; ++i) {
    double ideal = std::sin(2 * kPi * 440.0 * i / 44100.0) * 0.5 * 32767.0;
    worst = std::max(worst, std::fabs(s[(size_t)i * 2] - ideal));
  }
  CHECK(worst < 0.01 * 16384 + 2);
  // Downsampling 44100 -> 22050 keeps the pitch too.
  Mixer d = fullVolume(22050);
  d.playSound(makeTone(440, 1.0, 44100, 1, 0.5));
  std::vector<int16_t> t = mixFrames(d, 22050);
  CHECK(std::abs(upCrossings(t, 0, 22050) - 440) <= 1);
}

TEST(audio_mixer_music_streams_and_ends) {
  Mixer m = fullVolume();
  std::shared_ptr<PcmBuffer> b = makeBuffer(44100, 1, 5000, [](int i, int) { return i % 20000; });
  MusicParams p;
  p.loop = false;
  m.playMusic(makeBufferSource(b), p);
  CHECK(m.musicPlaying());
  std::vector<int16_t> s = mixFrames(m, 6000);
  for (int i = 0; i < 5000; ++i) CHECK_EQ(s[2 * i], (int16_t)(i % 20000));
  CHECK_EQ(s[2 * 5500], (int16_t)0);
  CHECK(!m.musicPlaying());
  CHECK_EQ(m.musicVoicesInUse(), 0);
}

TEST(audio_mixer_loop_is_seamless_on_a_synthetic_file) {
  // 100 Hz at 44100: 441 frames per period; a 4410-frame file is exactly ten periods.
  std::shared_ptr<PcmBuffer> tone = makeTone(100, 4410.0 / 44100.0, 44100, 1, 0.5);
  CHECK_EQ(tone->frames(), (int64_t)4410);
  Mixer m = fullVolume();
  m.playMusic(makeBufferSource(tone), MusicParams());
  std::vector<int16_t> s = mixFrames(m, 4410 * 5 + 123);
  for (size_t i = 0; i < s.size() / 2; ++i) CHECK_EQ(s[2 * i], tone->samples[i % 4410]);
  // With a loop start: an intro (ramp) then a section that repeats from frame 1000.
  std::shared_ptr<PcmBuffer> src = makeBuffer(44100, 1, 3000, [](int i, int) { return i < 1000 ? -i : (i - 1000) * 5; });
  Mixer m2 = fullVolume();
  MusicParams lp;
  lp.loopStartSeconds = 1000.0 / 44100.0;
  m2.playMusic(makeBufferSource(src), lp);
  s = mixFrames(m2, 3000 + 2 * 2000 + 17);
  for (int i = 0; i < 3000; ++i) CHECK_EQ(s[2 * i], src->samples[(size_t)i]);
  for (int i = 3000; i < 3000 + 4000; ++i) CHECK_EQ(s[2 * i], src->samples[(size_t)(1000 + (i - 1000) % 2000)]);
}

TEST(audio_mixer_loop_after_resampling_has_no_click) {
  // A 22.05 kHz sine of whole periods looped into a 44.1 kHz output: no step bigger than the sine's own slope.
  // 50 source frames per period at 441 Hz: 2200 frames = 44 whole periods.
  std::shared_ptr<PcmBuffer> tone = makeBuffer(22050, 1, 2200, [](int i, int) { return (int)std::lrint(std::sin(2 * kPi * i / 50.0) * 0.8 * 32767); });
  Mixer m = fullVolume();
  m.playMusic(makeBufferSource(tone), MusicParams());
  std::vector<int16_t> s = mixFrames(m, 4400 * 5);
  double maxStep = 0;
  for (size_t i = 1; i < s.size() / 2; ++i) maxStep = std::max(maxStep, (double)std::abs(s[2 * i] - s[2 * i - 2]));
  double slope = 2 * kPi * 441 / 44100.0 * 0.8 * 32767;  // largest change per output frame of the sine
  CHECK(maxStep <= slope * 1.02);
}

TEST(audio_mixer_cross_fade_keeps_the_energy_level) {
  // Two different tones (so they do not add up in phase) of equal level, cross-faded over one second.
  std::shared_ptr<PcmBuffer> a = makeTone(300, 4.0, 44100, 1, 0.5), b = makeTone(700, 4.0, 44100, 1, 0.5);
  Mixer m = fullVolume();
  m.playMusic(makeBufferSource(a), MusicParams());
  std::vector<int16_t> before = mixFrames(m, 44100);
  double base = rms(before, 4410, 44100);
  MusicParams p;
  p.fadeSeconds = 1.0f;
  m.playMusic(makeBufferSource(b), p);
  CHECK_EQ(m.musicVoicesInUse(), 2);
  std::vector<int16_t> fade = mixFrames(m, 44100);
  for (int w = 0; w < 10; ++w) {
    double r = rms(fade, w * 4410, (w + 1) * 4410);
    CHECK(r > base * 0.85 && r < base * 1.15);
  }
  // Nothing of A is left afterwards, and B plays at full level.
  CHECK_EQ(m.musicVoicesInUse(), 1);
  std::vector<int16_t> after = mixFrames(m, 8820);
  CHECK(std::fabs(rms(after, 0, 8820) - base) < base * 0.05);
  CHECK(std::abs(upCrossings(after, 0, 8820) - 140) <= 2);  // 700 Hz for 0.2 s
}

TEST(audio_mixer_fade_out_and_stop) {
  Mixer m = fullVolume();
  m.playMusic(makeBufferSource(makeTone(300, 4.0, 44100, 1, 0.5)), MusicParams());
  mixFrames(m, 4410);
  m.stopMusic(0.5f);
  CHECK(!m.musicPlaying());  // fading out counts as not playing
  std::vector<int16_t> s = mixFrames(m, 22050 + 200);
  CHECK(rms(s, 0, 2000) > rms(s, 18000, 20000) * 3);  // getting quieter
  CHECK(rms(s, 22100, 22250) == 0.0);                 // and gone
  CHECK_EQ(m.musicVoicesInUse(), 0);
  // Immediate stop.
  m.playMusic(makeBufferSource(makeTone(300, 4.0, 44100, 1, 0.5)), MusicParams());
  mixFrames(m, 100);
  m.stopMusic(0);
  CHECK_EQ(m.musicVoicesInUse(), 0);
  CHECK(rms(mixFrames(m, 100), 0, 100) == 0.0);
}

TEST(audio_mixer_music_level_zero_decodes_nothing) {
  Mixer m = fullVolume();
  int reads = 0;
  std::shared_ptr<PcmBuffer> b = makeTone(300, 2.0, 44100, 1, 0.5);
  m.setVolumes(0, 10);
  m.playMusic(std::unique_ptr<MusicSource>(new CountingSource(b, &reads)), MusicParams());
  std::vector<int16_t> s = mixFrames(m, 10000);
  CHECK_EQ(reads, 0);
  CHECK(rms(s, 0, 10000) == 0.0);
  m.setVolumes(10, 10);  // raised again: it starts decoding from where it stood
  s = mixFrames(m, 1000);
  CHECK(reads > 0);
  CHECK(rms(s, 0, 1000) > 1000);
}

TEST(audio_mixer_pause_and_resume_hold_the_position) {
  Mixer m = fullVolume();
  std::shared_ptr<PcmBuffer> ramp = makeBuffer(44100, 1, 30000, [](int i, int) { return i; });
  m.playMusic(makeBufferSource(ramp), MusicParams());
  std::vector<int16_t> a = mixFrames(m, 1000);
  CHECK_EQ(a[2 * 999], (int16_t)999);
  m.pauseMusic();
  CHECK(m.musicPaused() && !m.musicPlaying());
  std::vector<int16_t> p = mixFrames(m, 3000);
  for (int i = 1500; i < 3000; ++i) CHECK_EQ(p[2 * i], (int16_t)0);  // after the 20 ms ramp: silence
  int last = 0;
  for (int i = 0; i < 1500; ++i) last = std::max(last, (int)p[2 * i]);
  m.resumeMusic();
  std::vector<int16_t> r = mixFrames(m, 2000);
  // It goes on from where the pause stopped reading, never rewinding.
  CHECK(r[2 * 1999] > last);
  CHECK(r[2 * 1999] < 1000 + 1500 + 2000);
  for (int i = 100; i < 2000; ++i) CHECK(r[2 * i] >= r[2 * (i - 1)]);
}

TEST(audio_mixer_jingle_pauses_the_music_and_gives_it_back) {
  Mixer m = fullVolume();
  m.playMusic(makeBufferSource(makeBuffer(44100, 1, 1000, [](int, int) { return 1000; })), MusicParams());
  mixFrames(m, 500);
  MusicParams j;
  j.loop = false;
  j.pauseCurrent = true;
  m.playMusic(makeBufferSource(makeBuffer(44100, 1, 8820, [](int, int) { return 5000; })), j);  // 0.2 s
  std::vector<int16_t> s = mixFrames(m, 8820);
  CHECK_EQ(s[2 * 2000], (int16_t)5000);  // only the jingle (the old track has ramped out)
  CHECK_EQ(s[2 * 8000], (int16_t)5000);
  std::vector<int16_t> after = mixFrames(m, 3000);
  CHECK_EQ(after[2 * 2500], (int16_t)1000);  // the track is back (and loops on)
  CHECK(m.musicPlaying());
  CHECK_EQ(m.musicVoicesInUse(), 1);
}

TEST(audio_mixer_new_music_without_fade_replaces_at_once) {
  Mixer m = fullVolume();
  m.playMusic(makeBufferSource(makeBuffer(44100, 1, 1000, [](int, int) { return 1000; })), MusicParams());
  mixFrames(m, 100);
  m.playMusic(makeBufferSource(makeBuffer(44100, 1, 1000, [](int, int) { return 2000; })), MusicParams());
  std::vector<int16_t> s = mixFrames(m, 10);
  CHECK_EQ(s[0], (int16_t)2000);
  CHECK_EQ(m.musicVoicesInUse(), 1);
}

TEST(audio_mixer_output_does_not_depend_on_how_it_is_asked_for) {
  // Mixing 1280 frames in one call or in 64-frame pieces (the mixer's chunk) gives the same samples,
  // fades and all: tests and the device pump may ask for any amount.
  auto run = [](bool pieces) {
    Mixer m = fullVolume(44100);
    m.playMusic(makeBufferSource(makeTone(300, 1.0, 22050, 1, 0.5)), MusicParams());
    m.playSound(makeTone(1000, 0.5, 11025, 2, 0.5));
    std::vector<int16_t> out;
    std::vector<int16_t> part;
    MusicParams p;
    p.fadeSeconds = 0.02f;
    if (!pieces) {
      m.playMusic(makeBufferSource(makeTone(700, 1.0, 44100, 1, 0.5)), p);
      return mixFrames(m, 1280);
    }
    m.playMusic(makeBufferSource(makeTone(700, 1.0, 44100, 1, 0.5)), p);
    for (int i = 0; i < 20; ++i) {
      part = mixFrames(m, 64);
      out.insert(out.end(), part.begin(), part.end());
    }
    return out;
  };
  CHECK(run(false) == run(true));
}
