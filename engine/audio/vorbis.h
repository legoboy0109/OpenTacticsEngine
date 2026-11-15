// Ogg Vorbis (.ogg) from memory, through stb_vorbis. Damaged and truncated
// files give a message (stb_vorbis checks its input; tests/test_audio_damage.cpp
// feeds it garbage to keep it that way).
#pragma once
#include <string>

#include "audio/music_source.h"
#include "audio/pcm.h"

namespace ote {

// Whole decode (effects). Stops with an error as soon as the result would exceed
// maxBytes of 16-bit samples, so a huge file never fills the memory first.
bool decodeOgg(const uint8_t* data, size_t size, int64_t maxBytes, PcmBuffer& out, std::string& err);
// Streaming (music). The source keeps `file` alive.
std::unique_ptr<MusicSource> openOggStream(const FileBytes& file, std::string& err);

}  // namespace ote
