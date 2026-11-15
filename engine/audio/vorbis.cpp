#include "audio/vorbis.h"

#include <climits>

#include "audio/stb_vorbis_api.h"

namespace ote {
namespace {

std::string openError(int code) {
  switch (code) {
    case VORBIS_need_more_data: return "OGG: the file is cut off (not enough data)";
    case VORBIS_missing_capture_pattern:
    case VORBIS_invalid_stream: return "OGG: not an Ogg Vorbis file";
    case VORBIS_unexpected_eof: return "OGG: the file ends unexpectedly";
    case VORBIS_feature_not_supported: return "OGG: a feature of this file is not supported";
    case VORBIS_invalid_setup:
    case VORBIS_invalid_first_page:
    case VORBIS_invalid_stream_structure_version:
    case VORBIS_continued_packet_flag_invalid:
    case VORBIS_incorrect_stream_serial_number:
    case VORBIS_invalid_api_mixing:
    case VORBIS_seek_without_length:
    case VORBIS_cant_find_last_page:
    case VORBIS_seek_invalid:
    default: return "OGG: damaged file (stb_vorbis error " + std::to_string(code) + ")";
  }
}

// Opens the buffer and checks the format; null and a message on failure.
stb_vorbis* openChecked(const uint8_t* data, size_t size, int& rate, int& channels, std::string& err) {
  if (!data || size < 4 || size > (size_t)INT_MAX) { err = "OGG: the file is too short or too large"; return nullptr; }
  int code = 0;
  stb_vorbis* v = stb_vorbis_open_memory(data, (int)size, &code, nullptr);
  if (!v) { err = openError(code); return nullptr; }
  stb_vorbis_info info = stb_vorbis_get_info(v);
  rate = (int)info.sample_rate;
  channels = info.channels;
  if (channels < 1 || channels > 2) {
    err = "OGG: " + std::to_string(channels) + " channels (only mono and stereo are supported)";
  } else if (rate < kMinSampleRate || rate > kMaxSampleRate) {
    err = "OGG: sample rate " + std::to_string(rate) + " Hz (supported: 8000 to 48000)";
  } else {
    return v;
  }
  stb_vorbis_close(v);
  return nullptr;
}

class OggStream : public MusicSource {
 public:
  OggStream(const FileBytes& f, stb_vorbis* v, int rate, int ch) : file_(f), v_(v), rate_(rate), ch_(ch) {
    total_ = (int64_t)stb_vorbis_stream_length_in_samples64(v);
    if (total_ <= 0) total_ = -1;
  }
  ~OggStream() override { stb_vorbis_close(v_); }
  int rate() const override { return rate_; }
  int channels() const override { return ch_; }
  int64_t totalFrames() const override { return total_; }
  int read(int16_t* out, int maxFrames) override {
    if (maxFrames <= 0) return 0;
    return stb_vorbis_get_samples_short_interleaved(v_, ch_, out, maxFrames * ch_);
  }
  bool seek(int64_t frame) override {
    if (frame < 0 || frame > (int64_t)UINT_MAX) return false;
    return stb_vorbis_seek(v_, (unsigned)frame) != 0;
  }

 private:
  FileBytes file_;  // stb_vorbis reads straight from this buffer
  stb_vorbis* v_;
  int rate_, ch_;
  int64_t total_;
};

}  // namespace

bool decodeOgg(const uint8_t* data, size_t size, int64_t maxBytes, PcmBuffer& out, std::string& err) {
  int rate = 0, ch = 0;
  stb_vorbis* v = openChecked(data, size, rate, ch, err);
  if (!v) return false;
  bool ok = true;
  int64_t maxSamples = maxBytes / 2;
  // A declared length over the limit is refused before decoding anything.
  int64_t declared = (int64_t)stb_vorbis_stream_length_in_samples64(v);
  if (declared * ch > maxSamples) {
    err = "OGG: the sound is too long to load as an effect";
    ok = false;
  }
  out.rate = rate;
  out.channels = ch;
  out.samples.clear();
  int16_t block[4096 * 2];
  while (ok) {
    int n = stb_vorbis_get_samples_short_interleaved(v, ch, block, 4096 * ch);
    if (n <= 0) break;
    if ((int64_t)out.samples.size() + (int64_t)n * ch > maxSamples) {
      err = "OGG: the sound is too long to load as an effect";
      ok = false;
      break;
    }
    out.samples.insert(out.samples.end(), block, block + (size_t)n * ch);
  }
  stb_vorbis_close(v);
  if (ok && out.samples.empty()) { err = "OGG: the file holds no sound data"; ok = false; }
  if (!ok) out.samples.clear();
  return ok;
}

std::unique_ptr<MusicSource> openOggStream(const FileBytes& file, std::string& err) {
  if (!file) { err = "OGG: no data"; return nullptr; }
  int rate = 0, ch = 0;
  stb_vorbis* v = openChecked(file->data(), file->size(), rate, ch, err);
  if (!v) return nullptr;
  return std::unique_ptr<MusicSource>(new OggStream(file, v, rate, ch));
}

}  // namespace ote
