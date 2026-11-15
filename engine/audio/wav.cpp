#include "audio/wav.h"

#include <cstring>

namespace ote {
namespace {

uint32_t u32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
uint16_t u16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

// Converts `frames` frames of WAV data (8-bit unsigned or 16-bit little endian) to int16.
void convert(const uint8_t* src, int bits, int channels, int64_t frames, int16_t* out) {
  int64_t n = frames * channels;
  if (bits == 8) {
    for (int64_t i = 0; i < n; ++i) out[i] = (int16_t)(((int)src[i] - 128) * 256);
  } else {
    for (int64_t i = 0; i < n; ++i) out[i] = (int16_t)u16(src + 2 * i);
  }
}

class WavStream : public MusicSource {
 public:
  WavStream(const FileBytes& f, const WavInfo& i) : file_(f), info_(i) {}
  int rate() const override { return info_.rate; }
  int channels() const override { return info_.channels; }
  int64_t totalFrames() const override { return info_.frames(); }
  int read(int16_t* out, int maxFrames) override {
    int64_t left = info_.frames() - pos_;
    int64_t n = left < maxFrames ? left : maxFrames;
    if (n <= 0) return 0;
    int bytes = info_.channels * info_.bits / 8;
    convert(file_->data() + info_.dataOffset + (size_t)pos_ * bytes, info_.bits, info_.channels, n, out);
    pos_ += n;
    return (int)n;
  }
  bool seek(int64_t frame) override {
    if (frame < 0 || frame > info_.frames()) return false;
    pos_ = frame;
    return true;
  }

 private:
  FileBytes file_;
  WavInfo info_;
  int64_t pos_ = 0;
};

}  // namespace

bool parseWav(const uint8_t* data, size_t size, WavInfo& info, std::string& err) {
  info = WavInfo();
  if (!data || size < 12) { err = "WAV: the file is too short to be a sound"; return false; }
  if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0) {
    err = "WAV: not a WAV file (no RIFF/WAVE header)";
    return false;
  }
  bool haveFmt = false, haveData = false;
  size_t pos = 12;
  while (pos + 8 <= size) {
    const uint8_t* ck = data + pos;
    size_t len = u32(ck + 4), body = pos + 8;
    size_t avail = size - body;
    if (!memcmp(ck, "fmt ", 4)) {
      if (len < 16 || avail < 16) { err = "WAV: damaged fmt chunk (too short)"; return false; }
      int tag = u16(data + body);
      info.channels = u16(data + body + 2);
      info.rate = (int)u32(data + body + 4);
      info.bits = u16(data + body + 14);
      // WAVE_FORMAT_EXTENSIBLE carries the real format in its sub-format GUID (first two bytes).
      if (tag == 0xFFFE && len >= 40 && avail >= 40) tag = u16(data + body + 24);
      if (tag != 1) {
        err = "WAV: format " + std::to_string(tag) + " is not supported (only uncompressed PCM)";
        return false;
      }
      if (info.channels < 1 || info.channels > 2) {
        err = "WAV: " + std::to_string(info.channels) + " channels (only mono and stereo are supported)";
        return false;
      }
      if (info.bits != 8 && info.bits != 16) {
        err = "WAV: " + std::to_string(info.bits) + "-bit samples (only 8 and 16 are supported)";
        return false;
      }
      if (info.rate < kMinSampleRate || info.rate > kMaxSampleRate) {
        err = "WAV: sample rate " + std::to_string(info.rate) + " Hz (supported: 8000 to 48000)";
        return false;
      }
      haveFmt = true;
    } else if (!memcmp(ck, "data", 4) && !haveData) {
      // A truncated file (or a recorder that left the size at 0 / 0xFFFFFFFF) keeps what is there.
      info.dataOffset = body;
      info.dataBytes = len < avail ? len : avail;
      haveData = true;
    }
    if (len > avail) break;  // the chunk runs past the end: nothing can follow it
    pos = body + len + (len & 1);
  }
  if (!haveFmt) { err = "WAV: no fmt chunk (damaged header)"; return false; }
  if (!haveData) { err = "WAV: no data chunk (damaged header)"; return false; }
  size_t frameBytes = (size_t)(info.channels * info.bits / 8);
  info.dataBytes -= info.dataBytes % frameBytes;
  if (info.dataBytes == 0) { err = "WAV: the file holds no sound data"; return false; }
  return true;
}

bool decodeWav(const uint8_t* data, size_t size, PcmBuffer& out, std::string& err) {
  WavInfo info;
  if (!parseWav(data, size, info, err)) return false;
  out.rate = info.rate;
  out.channels = info.channels;
  out.samples.resize((size_t)info.frames() * info.channels);
  convert(data + info.dataOffset, info.bits, info.channels, info.frames(), out.samples.data());
  return true;
}

std::unique_ptr<MusicSource> openWavStream(const FileBytes& file, std::string& err) {
  WavInfo info;
  if (!file || !parseWav(file->data(), file->size(), info, err)) return nullptr;
  return std::unique_ptr<MusicSource>(new WavStream(file, info));
}

}  // namespace ote
