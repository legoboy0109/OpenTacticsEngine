// --audio-render FILE --seconds N: runs the sound self-test through the real AudioService and mixer
// against a device that only records, and writes the samples as a 16-bit stereo WAV. No window, no
// sound device: the way to check mixing offline (tests/audio_player_test.py).
#pragma once
#include "args.h"

namespace ote {

// Process exit code: 0 on success, 1 (message on stderr) when the file cannot be written.
int runAudioRender(const Args& args);

}  // namespace ote
