// Reads a sound file into memory (the decoders never touch files, so the same decoders work on every
// platform; on the web this reads Emscripten's in-memory file system like every other file).
#pragma once
#include <string>

#include "audio/music_source.h"

namespace ote {

// Null and a plain message in err when the file cannot be read or is absurdly large.
FileBytes readAudioFile(const std::string& path, std::string& err);

}  // namespace ote
