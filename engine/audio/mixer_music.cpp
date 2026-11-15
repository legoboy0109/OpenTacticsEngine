// The two music voices of the mixer: streaming, looping, cross-fades, pause.
#include <cmath>
#include <cstring>

#include "audio/mixer.h"

namespace ote {
namespace {

const float kHalfPi = 1.57079632679f;
const float kPauseRampSeconds = 0.02f;  // a pause fades over 20 ms instead of clicking

// Gain of a fade level: equal power (sin), exactly 1 at full level so an unfaded voice is untouched.
float fadeGain(float level) { return level >= 1.0f ? 1.0f : level <= 0.0f ? 0.0f : sinf(level * kHalfPi); }

}  // namespace

void Mixer::startFade(MusicVoice& v, float target, float seconds) {
  v.target = target;
  if (seconds <= 0.0f) {
    v.level = target;
    v.inc = 0;
  } else {
    v.inc = 1.0f / (seconds * (float)rate_);
  }
}

void Mixer::killMusicVoice(MusicVoice& v) {
  v.active = false;
  v.source.reset();
  v.paused = v.pausedForJingle = false;
  v.have = 0;
}

int Mixer::freeMusicVoice() {
  for (int i = 0; i < kMusicVoices; ++i)
    if (!music_[i].active) return i;
  return 0;
}

void Mixer::playMusic(std::unique_ptr<MusicSource> source, const MusicParams& p) {
  if (!source) return;
  int ch = source->channels();
  int srate = source->rate();
  if (ch < 1 || ch > 2 || srate < 1) return;  // decoders never give this; a bad custom source is dropped

  int heldForJingle = -1;
  for (int i = 0; i < kMusicVoices; ++i)
    if (music_[i].active && music_[i].pausedForJingle) heldForJingle = i;

  int target;
  if (p.pauseCurrent && current_ >= 0 && heldForJingle < 0) {
    // A jingle: the current music holds still, the jingle takes the other voice.
    MusicVoice& cur = music_[current_];
    cur.paused = true;
    cur.pausedForJingle = true;
    target = 1 - current_;
    killMusicVoice(music_[target]);  // a voice still fading out is cut
  } else if (p.pauseCurrent) {
    // A jingle while another jingle plays (or nothing plays): replace in place, hold nothing new.
    target = current_ >= 0 ? current_ : freeMusicVoice();
    if (target == heldForJingle) target = 1 - target;
    killMusicVoice(music_[target]);
  } else {
    // Plain change of music: the old one fades out, anything held for a jingle is dropped.
    if (heldForJingle >= 0) killMusicVoice(music_[heldForJingle]);
    int old = current_;
    bool crossFade = old >= 0 && p.fadeSeconds > 0;
    if (crossFade) {
      music_[old].paused = false;
      startFade(music_[old], 0.0f, p.fadeSeconds);
      target = 1 - old;
    } else {
      if (old >= 0) killMusicVoice(music_[old]);
      target = old >= 0 ? old : freeMusicVoice();
    }
    killMusicVoice(music_[target]);  // a leftover voice still fading out is cut
  }

  MusicVoice& v = music_[target];
  v.source = std::move(source);
  v.active = true;
  v.loop = p.loop;
  v.loopStartFrame = (int64_t)llround(p.loopStartSeconds * srate);
  if (v.loopStartFrame < 0) v.loopStartFrame = 0;
  v.volume = p.volume < 0 ? 0 : p.volume;
  v.channels = ch;
  v.step = (double)srate / (double)rate_;
  v.have = 0;
  v.pos = 0;
  v.eof = false;
  v.paused = false;
  v.pausedForJingle = false;
  v.pauseGain = 1.0f;
  v.level = 0.0f;
  startFade(v, 1.0f, p.fadeSeconds);  // no fade: jumps to full level
  current_ = target;
}

void Mixer::stopMusic(float fadeSeconds) {
  for (MusicVoice& v : music_) {
    if (!v.active) continue;
    if (v.pausedForJingle || fadeSeconds <= 0) { killMusicVoice(v); continue; }
    v.paused = false;  // a paused voice would never finish fading
    startFade(v, 0.0f, fadeSeconds);
  }
  current_ = -1;
}

void Mixer::pauseMusic() {
  if (current_ >= 0) music_[current_].paused = true;
}

void Mixer::resumeMusic() {
  for (MusicVoice& v : music_) v.paused = v.pausedForJingle = false;
}

bool Mixer::musicPlaying() const {
  for (const MusicVoice& v : music_)
    if (v.active && !v.paused && v.target > 0.0f) return true;
  return false;
}

bool Mixer::musicPaused() const {
  for (const MusicVoice& v : music_)
    if (v.active && v.paused) return true;
  return false;
}

int Mixer::musicVoicesInUse() const {
  int n = 0;
  for (const MusicVoice& v : music_) n += v.active ? 1 : 0;
  return n;
}

// A music voice's source ran out (not looping): free it, and if a jingle was holding another
// track, let that one go on.
void Mixer::onMusicEnded(int voice) {
  killMusicVoice(music_[voice]);
  if (current_ == voice) current_ = -1;
  for (int i = 0; i < kMusicVoices; ++i) {
    if (music_[i].active && music_[i].pausedForJingle) {
      music_[i].paused = music_[i].pausedForJingle = false;
      current_ = i;
    }
  }
}

// Reads more source frames behind the ones still needed. At the end of a looping source it jumps to
// loopStart and goes on appending, so the seam is as smooth as the file itself.
void Mixer::refill(MusicVoice& v) {
  const int ch = v.channels;
  int drop = (int)v.pos;
  if (drop > v.have) drop = v.have;
  if (drop > 0) {
    memmove(v.block.data(), v.block.data() + (size_t)drop * ch, (size_t)(v.have - drop) * ch * sizeof(int16_t));
    v.have -= drop;
    v.pos -= drop;
  }
  int room = kMusicBlockFrames - v.have;
  int got = v.source->read(v.block.data() + (size_t)v.have * ch, room);
  if (got <= 0 && v.loop && v.source->seek(v.loopStartFrame))
    got = v.source->read(v.block.data() + (size_t)v.have * ch, room);
  if (got <= 0) { v.eof = true; return; }
  if (got > room) got = room;  // a misbehaving source must not move the end of the block
  v.have += got;
}

bool Mixer::ensureFrames(MusicVoice& v) {
  for (;;) {
    int i = (int)v.pos;
    if (i + 1 < v.have) return true;
    if (v.eof) return i < v.have;  // the last frame is interpolated with itself
    refill(v);
  }
}

void Mixer::mixMusicVoice(MusicVoice& v, float* acc, int n) {
  // Gain edges of this chunk: fade level and pause ramp each move linearly across it.
  float l0 = v.level, l1 = l0;
  if (l0 < v.target) { l1 = l0 + v.inc * n; if (l1 > v.target) l1 = v.target; }
  else if (l0 > v.target) { l1 = l0 - v.inc * n; if (l1 < v.target) l1 = v.target; }
  float p0 = v.pauseGain;
  float pstep = (float)n / (kPauseRampSeconds * (float)rate_);
  float p1 = v.paused ? (p0 - pstep < 0 ? 0 : p0 - pstep) : (p0 + pstep > 1 ? 1 : p0 + pstep);
  if (v.paused && p0 <= 0.0f) return;  // held still: nothing decoded, nothing heard, the fade waits too
  float g0 = fadeGain(l0) * p0, g1 = fadeGain(l1) * p1;
  float base = v.volume * musicGain_;
  int index = (int)(&v - music_);
  const int ch = v.channels;
  bool ended = false;
  for (int k = 0; k < n; ++k) {
    if (!ensureFrames(v)) { ended = true; break; }
    int i = (int)v.pos;
    float f = (float)(v.pos - (double)i);
    const int16_t* a = v.block.data() + (size_t)i * ch;
    const int16_t* b = (i + 1 < v.have) ? a + ch : a;
    float g = (g0 + (g1 - g0) * (float)(k + 1) / (float)n) * base;
    if (ch == 1) {
      float s = ((float)a[0] + (float)(b[0] - a[0]) * f) * g;
      acc[2 * k] += s;
      acc[2 * k + 1] += s;
    } else {
      acc[2 * k] += ((float)a[0] + (float)(b[0] - a[0]) * f) * g;
      acc[2 * k + 1] += ((float)a[1] + (float)(b[1] - a[1]) * f) * g;
    }
    v.pos += v.step;
  }
  if (ended) { onMusicEnded(index); return; }
  v.level = l1;
  v.pauseGain = p1;
  if (v.target <= 0.0f && v.level <= 0.0f) {  // faded out
    killMusicVoice(v);
    if (current_ == index) current_ = -1;
  }
}

void Mixer::mixMusic(float* acc, int n) {
  if (musicGain_ <= 0.0f) return;  // level 0: nothing is decoded
  for (int i = 0; i < kMusicVoices; ++i)
    if (music_[i].active) mixMusicVoice(music_[i], acc, n);
}

}  // namespace ote
