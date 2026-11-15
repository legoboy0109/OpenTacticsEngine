// WAV (RIFF, 8/16-bit PCM, mono/stereo) from memory. The header is checked
// chunk by chunk against the real size of the buffer: a damaged or truncated
// file gives a plain message, never an over-read.
#pragma once
#include <string>

#include "audio/music_source.h"
#include "audio/pcm.h"

namespace ote {

// Where the samples of a WAV are (offsets into the caller's buffer).
struct WavInfo {
  int rate = 0, channels = 0, bits = 0;
  size_t dataOffset = 0, dataBytes = 0;  // dataBytes: whole frames only, clamped to the buffer
  int64_t frames() const { return channels ? (int64_t)(dataBytes / (size_t)(channels * bits / 8)) : 0; }
};

bool parseWav(const uint8_t* data, size_t size, WavInfo& info, std::string& err);
// Whole decode (effects). The effect limits are NOT checked here (see decode.h).
bool decodeWav(const uint8_t* data, size_t size, PcmBuffer& out, std::string& err);
// Streaming (music).
std::unique_ptr<MusicSource> openWavStream(const FileBytes& file, std::string& err);

}  // namespace ote
