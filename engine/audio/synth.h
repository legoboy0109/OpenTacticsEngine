// Sounds made in code: the options screen's tick, the self-test melody, plain
// test tones, and a WAV writer. Nothing here needs a file or a device.
#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "audio/music_source.h"
#include "audio/pcm.h"

namespace ote {

// A sine wave of exactly `seconds` (rounded to whole frames), full scale times `amplitude` (0..1).
std::shared_ptr<PcmBuffer> makeTone(double freqHz, double seconds, int rate, int channels = 1, double amplitude = 0.5);
// The short click the options screen plays when a volume slider moves.
std::shared_ptr<PcmBuffer> makeTick();
// A short two-note chime (the "effect" of --audio-test).
std::shared_ptr<PcmBuffer> makeChime();
// A looping four-bar tune of square-wave notes (the "music" of --audio-test): `bars` repeats of C-E-G-E.
std::shared_ptr<PcmBuffer> makeMelody(int bars = 4);

// A MusicSource that plays a buffer (so music code can be exercised without a file).
std::unique_ptr<MusicSource> makeBufferSource(const std::shared_ptr<const PcmBuffer>& buffer);

// A canonical 44-byte-header 16-bit PCM WAV file image.
std::vector<uint8_t> encodeWav16(const int16_t* samples, size_t frames, int channels, int rate);
// The same with 8-bit unsigned samples (for tests of the 8-bit reader).
std::vector<uint8_t> encodeWav8(const uint8_t* samples, size_t frames, int channels, int rate);

}  // namespace ote
