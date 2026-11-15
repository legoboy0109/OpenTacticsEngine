// The two entry points the player uses: whole effects and streamed music. The
// format is told from the first bytes (RIFF = WAV, OggS = Ogg Vorbis), not the
// file name. Everything works on memory buffers (the caller reads the file
// through the platform), so there is no file access in the decoders.
#pragma once
#include <string>

#include "audio/music_source.h"
#include "audio/pcm.h"

namespace ote {

// Decodes a whole sound effect. Refuses (false, plain message in err) anything
// longer than kMaxEffectSeconds or bigger than kMaxEffectBytes decoded, and
// everything damaged or unsupported.
bool decodeSound(const uint8_t* data, size_t size, PcmBuffer& out, std::string& err);

// Opens music for streaming; null and a message on failure. No length limit.
std::unique_ptr<MusicSource> openMusic(const FileBytes& file, std::string& err);

}  // namespace ote
