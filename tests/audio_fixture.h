// Helpers for the audio tests: tiny sources and "mix into memory".
#pragma once
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "audio/mixer.h"
#include "audio/synth.h"

namespace audio_test {

// Mixes `frames` frames at once; returns interleaved stereo.
inline std::vector<int16_t> mixFrames(ote::Mixer& m, int frames) {
  std::vector<int16_t> out((size_t)frames * 2);
  m.mix(out.data(), frames);
  return out;
}

// A buffer whose sample i is value(i): exact numbers to compare against.
template <class F>
std::shared_ptr<ote::PcmBuffer> makeBuffer(int rate, int channels, int frames, F value) {
  std::shared_ptr<ote::PcmBuffer> b(new ote::PcmBuffer());
  b->rate = rate;
  b->channels = channels;
  b->samples.resize((size_t)frames * channels);
  for (int i = 0; i < frames; ++i)
    for (int c = 0; c < channels; ++c) b->samples[(size_t)i * channels + c] = (int16_t)value(i, c);
  return b;
}

// Root mean square of one channel over [from, to) frames.
inline double rms(const std::vector<int16_t>& stereo, int from, int to, int channel = 0) {
  double sum = 0;
  for (int i = from; i < to; ++i) {
    double s = stereo[(size_t)i * 2 + channel];
    sum += s * s;
  }
  return std::sqrt(sum / (to - from));
}

// Counts sign changes (upward zero crossings) of one channel.
inline int upCrossings(const std::vector<int16_t>& stereo, int from, int to, int channel = 0) {
  int n = 0;
  for (int i = from + 1; i < to; ++i)
    if (stereo[(size_t)(i - 1) * 2 + channel] < 0 && stereo[(size_t)i * 2 + channel] >= 0) ++n;
  return n;
}

// A music source that counts how much was decoded from it.
class CountingSource : public ote::MusicSource {
 public:
  CountingSource(std::shared_ptr<const ote::PcmBuffer> b, int* reads) : inner_(ote::makeBufferSource(b)), reads_(reads) {}
  int rate() const override { return inner_->rate(); }
  int channels() const override { return inner_->channels(); }
  int64_t totalFrames() const override { return inner_->totalFrames(); }
  int read(int16_t* out, int n) override { ++*reads_; return inner_->read(out, n); }
  bool seek(int64_t f) override { return inner_->seek(f); }

 private:
  std::unique_ptr<ote::MusicSource> inner_;
  int* reads_;
};

}  // namespace audio_test
