// Decoded sound data shared by the decoders, the mixer and the player's audio
// service. No backend or device headers anywhere in engine/audio (docs/SOUND.md).
#pragma once
#include <cstdint>
#include <vector>

namespace ote {

// A whole sound effect: 16-bit samples, interleaved when stereo.
struct PcmBuffer {
  int rate = 0;      // frames per second of the data (8000..48000)
  int channels = 0;  // 1 or 2
  std::vector<int16_t> samples;
  int64_t frames() const { return channels > 0 ? (int64_t)samples.size() / channels : 0; }
};

// What the loaders accept (docs/SOUND.md "Files and formats").
constexpr int kMinSampleRate = 8000;
constexpr int kMaxSampleRate = 48000;
constexpr double kMaxEffectSeconds = 10.0;
constexpr int64_t kMaxEffectBytes = 2 * 1024 * 1024;  // decoded, 16-bit

}  // namespace ote
