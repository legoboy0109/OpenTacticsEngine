#include "audio/decode.h"

#include <cstring>

#include "audio/vorbis.h"
#include "audio/wav.h"

namespace ote {
namespace {

enum Kind { kWav, kOgg, kUnknown };

Kind kindOf(const uint8_t* d, size_t n) {
  if (n >= 4 && !memcmp(d, "RIFF", 4)) return kWav;
  if (n >= 4 && !memcmp(d, "OggS", 4)) return kOgg;
  return kUnknown;
}

const char* kUnknownMessage = "unknown sound format (use .ogg Ogg Vorbis or .wav PCM)";

}  // namespace

bool decodeSound(const uint8_t* data, size_t size, PcmBuffer& out, std::string& err) {
  out = PcmBuffer();
  Kind k = kindOf(data, size);
  if (k == kUnknown) { err = kUnknownMessage; return false; }
  if (k == kWav) {
    WavInfo info;
    if (!parseWav(data, size, info, err)) return false;
    // Check the limits on the header's numbers first: nothing is allocated for a refused file.
    if ((double)info.frames() / info.rate > kMaxEffectSeconds) {
      err = "the sound is longer than 10 seconds (long sounds belong in music)";
      return false;
    }
    if (info.frames() * info.channels * 2 > kMaxEffectBytes) {
      err = "the sound is larger than 2 MB once decoded";
      return false;
    }
    return decodeWav(data, size, out, err);
  }
  if (!decodeOgg(data, size, kMaxEffectBytes, out, err)) return false;
  if ((double)out.frames() / out.rate > kMaxEffectSeconds) {
    err = "the sound is longer than 10 seconds (long sounds belong in music)";
    out = PcmBuffer();
    return false;
  }
  return true;
}

std::unique_ptr<MusicSource> openMusic(const FileBytes& file, std::string& err) {
  if (!file) { err = "no data"; return nullptr; }
  Kind k = kindOf(file->data(), file->size());
  if (k == kWav) return openWavStream(file, err);
  if (k == kOgg) return openOggStream(file, err);
  err = kUnknownMessage;
  return nullptr;
}

}  // namespace ote
