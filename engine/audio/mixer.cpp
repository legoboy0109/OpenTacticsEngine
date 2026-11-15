// Mixer core and the effect voices; the music voices are in mixer_music.cpp.
#include "audio/mixer.h"

#include <cmath>

namespace ote {
namespace {

// Frames mixed per step. Gains (fades, pause ramps) are interpolated linearly across a chunk, which
// is smooth enough to be inaudible and keeps the per-frame work to a multiply.
const int kChunk = 64;

int clampLevel(int v) { return v < 0 ? 0 : v > 10 ? 10 : v; }

}  // namespace

float volumeGain(int level) {
  level = clampLevel(level);
  float x = (float)level / 10.0f;
  return x * x;
}

Mixer::Mixer(int outRate) : rate_(outRate < 1 ? 1 : outRate) {
  musicGain_ = volumeGain(musicLevel_);
  soundGain_ = volumeGain(soundLevel_);
  acc_.assign((size_t)kChunk * 2, 0.0f);
  for (MusicVoice& v : music_) v.block.assign((size_t)kMusicBlockFrames * 2, 0);
}

void Mixer::setVolumes(int musicLevel, int soundLevel) {
  musicLevel_ = clampLevel(musicLevel);
  soundLevel_ = clampLevel(soundLevel);
  musicGain_ = volumeGain(musicLevel_);
  soundGain_ = volumeGain(soundLevel_);
}

int Mixer::playSound(const std::shared_ptr<const PcmBuffer>& buffer, float volume, bool onMusicBus) {
  if (!buffer || buffer->frames() <= 0 || buffer->rate <= 0) return -1;
  if (buffer->channels < 1 || buffer->channels > 2) return -1;
  if ((onMusicBus ? musicGain_ : soundGain_) <= 0.0f) return -1;
  int pick = -1;
  for (int i = 0; i < kSoundVoices; ++i)
    if (!sounds_[i].active) { pick = i; break; }
  if (pick < 0) {  // every voice is busy: cut the oldest
    pick = 0;
    for (int i = 1; i < kSoundVoices; ++i)
      if (sounds_[i].seq < sounds_[pick].seq) pick = i;
  }
  SoundVoice& v = sounds_[pick];
  v.buffer = buffer;
  v.pos = 0;
  v.step = (double)buffer->rate / (double)rate_;
  v.volume = volume < 0 ? 0 : volume;
  v.onMusicBus = onMusicBus;
  v.seq = nextSeq_++;
  v.active = true;
  return pick;
}

void Mixer::stopSounds() {
  for (SoundVoice& v : sounds_) {
    v.active = false;
    v.buffer.reset();
  }
}

int Mixer::activeSounds() const {
  int n = 0;
  for (const SoundVoice& v : sounds_) n += v.active ? 1 : 0;
  return n;
}

void Mixer::mixSounds(float* acc, int n) {
  for (SoundVoice& v : sounds_) {
    if (!v.active) continue;
    const PcmBuffer& b = *v.buffer;
    const int64_t total = b.frames();
    const int16_t* d = b.samples.data();
    const float g = v.volume * (v.onMusicBus ? musicGain_ : soundGain_);
    for (int k = 0; k < n; ++k) {
      int64_t i = (int64_t)v.pos;
      if (i >= total) {
        v.active = false;
        v.buffer.reset();
        break;
      }
      // Linear interpolation between this source frame and the next (the last one repeats).
      float f = (float)(v.pos - (double)i);
      int64_t j = i + 1 < total ? i + 1 : i;
      if (b.channels == 1) {
        float s = (float)d[i] + (float)(d[j] - d[i]) * f;
        acc[2 * k] += s * g;
        acc[2 * k + 1] += s * g;
      } else {
        float l = (float)d[2 * i] + (float)(d[2 * j] - d[2 * i]) * f;
        float r = (float)d[2 * i + 1] + (float)(d[2 * j + 1] - d[2 * i + 1]) * f;
        acc[2 * k] += l * g;
        acc[2 * k + 1] += r * g;
      }
      v.pos += v.step;
    }
  }
}

void Mixer::mixChunk(int16_t* out, int n) {
  float* acc = acc_.data();
  for (int i = 0; i < n * 2; ++i) acc[i] = 0.0f;
  mixSounds(acc, n);
  mixMusic(acc, n);
  // Saturate (clip hard) and round to nearest.
  for (int i = 0; i < n * 2; ++i) {
    float s = acc[i];
    int v = (int)(s < 0 ? s - 0.5f : s + 0.5f);
    out[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
  }
}

void Mixer::mix(int16_t* out, int frames) {
  while (frames > 0) {
    int n = frames < kChunk ? frames : kChunk;
    mixChunk(out, n);
    out += n * 2;
    frames -= n;
  }
}

}  // namespace ote
