#include "audio/synth.h"

#include <cmath>
#include <cstring>

namespace ote {
namespace {

const double kPi = 3.14159265358979323846;

void put16(std::vector<uint8_t>& v, unsigned x) { v.push_back((uint8_t)x); v.push_back((uint8_t)(x >> 8)); }
void put32(std::vector<uint8_t>& v, uint32_t x) { put16(v, x & 0xFFFF); put16(v, x >> 16); }
void putTag(std::vector<uint8_t>& v, const char* t) { v.insert(v.end(), t, t + 4); }

std::vector<uint8_t> wavHeader(size_t dataBytes, int channels, int rate, int bits) {
  std::vector<uint8_t> v;
  putTag(v, "RIFF");
  put32(v, (uint32_t)(36 + dataBytes));
  putTag(v, "WAVE");
  putTag(v, "fmt ");
  put32(v, 16);
  put16(v, 1);
  put16(v, (unsigned)channels);
  put32(v, (uint32_t)rate);
  put32(v, (uint32_t)(rate * channels * bits / 8));
  put16(v, (unsigned)(channels * bits / 8));
  put16(v, (unsigned)bits);
  putTag(v, "data");
  put32(v, (uint32_t)dataBytes);
  return v;
}

// Adds a square-wave note with a short attack/release (no clicks) at `start` (in frames).
void addNote(PcmBuffer& b, double freq, double startSec, double lenSec, double amp) {
  int64_t start = (int64_t)(startSec * b.rate), n = (int64_t)(lenSec * b.rate);
  for (int64_t i = 0; i < n && start + i < b.frames(); ++i) {
    double t = (double)i / b.rate;
    double env = std::fmin(1.0, std::fmin(t / 0.01, (lenSec - t) / 0.03));
    double phase = std::fmod(t * freq, 1.0);
    double s = (phase < 0.5 ? 1.0 : -1.0) * env * amp * 32767.0;
    b.samples[(size_t)(start + i)] = (int16_t)std::lrint(s);
  }
}

class BufferSource : public MusicSource {
 public:
  explicit BufferSource(const std::shared_ptr<const PcmBuffer>& b) : b_(b) {}
  int rate() const override { return b_->rate; }
  int channels() const override { return b_->channels; }
  int64_t totalFrames() const override { return b_->frames(); }
  int read(int16_t* out, int maxFrames) override {
    int64_t left = b_->frames() - pos_;
    int n = (int)(left < maxFrames ? left : maxFrames);
    if (n <= 0) return 0;
    memcpy(out, b_->samples.data() + (size_t)pos_ * b_->channels, (size_t)n * b_->channels * sizeof(int16_t));
    pos_ += n;
    return n;
  }
  bool seek(int64_t frame) override {
    if (frame < 0 || frame > b_->frames()) return false;
    pos_ = frame;
    return true;
  }

 private:
  std::shared_ptr<const PcmBuffer> b_;
  int64_t pos_ = 0;
};

}  // namespace

std::shared_ptr<PcmBuffer> makeTone(double freqHz, double seconds, int rate, int channels, double amplitude) {
  std::shared_ptr<PcmBuffer> b(new PcmBuffer());
  b->rate = rate;
  b->channels = channels;
  int64_t n = (int64_t)std::llround(seconds * rate);
  b->samples.resize((size_t)(n * channels));
  for (int64_t i = 0; i < n; ++i) {
    int16_t s = (int16_t)std::lrint(std::sin(2.0 * kPi * freqHz * (double)i / rate) * amplitude * 32767.0);
    for (int c = 0; c < channels; ++c) b->samples[(size_t)(i * channels + c)] = s;
  }
  return b;
}

std::shared_ptr<PcmBuffer> makeTick() {
  // 40 ms of a 1.2 kHz sine with a fast exponential decay: clearly a click, not a beep.
  std::shared_ptr<PcmBuffer> b = makeTone(1200, 0.04, 22050, 1, 0.6);
  for (size_t i = 0; i < b->samples.size(); ++i) {
    double t = (double)i / b->rate;
    b->samples[i] = (int16_t)std::lrint(b->samples[i] * std::exp(-t * 90.0));
  }
  return b;
}

std::shared_ptr<PcmBuffer> makeChime() {
  std::shared_ptr<PcmBuffer> b(new PcmBuffer());
  b->rate = 22050;
  b->channels = 1;
  b->samples.assign((size_t)(0.5 * b->rate), 0);
  addNote(*b, 880.0, 0.0, 0.18, 0.4);
  addNote(*b, 1318.5, 0.16, 0.34, 0.4);
  return b;
}

std::shared_ptr<PcmBuffer> makeMelody(int bars) {
  std::shared_ptr<PcmBuffer> b(new PcmBuffer());
  b->rate = 22050;
  b->channels = 1;
  const double beat = 0.25;                                     // seconds per note
  const double notes[4] = {261.63, 329.63, 392.0, 329.63};      // C4 E4 G4 E4
  b->samples.assign((size_t)(bars * 4 * beat * b->rate), 0);
  for (int bar = 0; bar < bars; ++bar)
    for (int k = 0; k < 4; ++k) addNote(*b, notes[k], (bar * 4 + k) * beat, beat * 0.9, 0.25);
  return b;
}

std::unique_ptr<MusicSource> makeBufferSource(const std::shared_ptr<const PcmBuffer>& buffer) {
  return std::unique_ptr<MusicSource>(new BufferSource(buffer));
}

std::vector<uint8_t> encodeWav16(const int16_t* samples, size_t frames, int channels, int rate) {
  std::vector<uint8_t> v = wavHeader(frames * channels * 2, channels, rate, 16);
  for (size_t i = 0; i < frames * channels; ++i) put16(v, (uint16_t)samples[i]);
  return v;
}

std::vector<uint8_t> encodeWav8(const uint8_t* samples, size_t frames, int channels, int rate) {
  std::vector<uint8_t> v = wavHeader(frames * channels, channels, rate, 8);
  v.insert(v.end(), samples, samples + frames * channels);
  return v;
}

}  // namespace ote
