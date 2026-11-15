// What the helper can tell about a sound file from its bytes (docs/EDITOR_API.md "Audio files"): which kind it is,
// and for the list the length, rate and channels. This is only information for the editor; the engine's own loader
// (run by /problems) decides what is acceptable.
#pragma once
#include <string>

namespace ote_editor {

enum class AudioKind { Unknown, Wav, Ogg };

// "RIFF....WAVE" is a WAV, "OggS" an Ogg file. The file name is never consulted.
AudioKind sniffAudio(const std::string& bytes);

struct AudioFacts {
  bool ok = false;
  std::string error;    // when !ok: why the header could not be read
  double seconds = -1;  // -1 = unknown
  int rate = 0;         // Hz
  int channels = 0;
};

// WAV: from the "fmt " and "data" chunks (a data size of 0, 0xFFFFFFFF or past the end means "to the end of the file",
// like the engine). OGG Vorbis: rate and channels from the identification header, length from the granule position of
// the last page (looked for in the final 64 KB).
AudioFacts readAudioFacts(const std::string& bytes, AudioKind kind);

}  // namespace ote_editor
