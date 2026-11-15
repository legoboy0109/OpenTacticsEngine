#include "audio_info.h"

#include <cstdint>
#include <cstring>

namespace ote_editor {

namespace {

uint32_t le32(const unsigned char* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
uint16_t le16(const unsigned char* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint64_t le64(const unsigned char* p) { return (uint64_t)le32(p) | ((uint64_t)le32(p + 4) << 32); }

AudioFacts fail(const std::string& why) {
  AudioFacts f;
  f.error = why;
  return f;
}

AudioFacts wavFacts(const std::string& bytes) {
  const unsigned char* b = (const unsigned char*)bytes.data();
  const size_t n = bytes.size();
  size_t pos = 12;
  int rate = 0, channels = 0, blockAlign = 0;
  bool haveFmt = false;
  while (pos + 8 <= n) {
    const uint32_t size = le32(b + pos + 4);
    const size_t body = pos + 8;
    if (std::memcmp(b + pos, "fmt ", 4) == 0) {
      if (size < 16 || body + 16 > n) return fail("the WAV header is cut short");
      channels = le16(b + body + 2);
      rate = (int)le32(b + body + 4);
      blockAlign = le16(b + body + 12);
      haveFmt = true;
    } else if (std::memcmp(b + pos, "data", 4) == 0) {
      if (!haveFmt) return fail("the WAV file has no format chunk before its sound data");
      if (channels < 1 || rate < 1 || blockAlign < 1) return fail("the WAV header has no sound format");
      const size_t avail = n - body;
      const size_t len = (size == 0 || size == 0xFFFFFFFFu || size > avail) ? avail : size;
      AudioFacts f;
      f.ok = true;
      f.rate = rate;
      f.channels = channels;
      f.seconds = (double)(len / (size_t)blockAlign) / (double)rate;
      return f;
    }
    // Chunks are padded to an even size.
    const uint64_t next = (uint64_t)body + size + (size & 1u);
    if (next > n) break;
    pos = (size_t)next;
  }
  return fail(haveFmt ? "the WAV file has no sound data" : "the WAV file has no format chunk");
}

AudioFacts oggFacts(const std::string& bytes) {
  const unsigned char* b = (const unsigned char*)bytes.data();
  const size_t n = bytes.size();
  // First page: 27 byte header, segment table, then the first packet: 0x01 "vorbis" version(4) channels(1) rate(4).
  if (n < 28) return fail("the Ogg file is cut short");
  const size_t pkt = 27 + (size_t)b[26];
  if (pkt + 16 > n || std::memcmp(b + pkt, "\x01vorbis", 7) != 0)
    return fail("this Ogg file is not Ogg Vorbis (only Vorbis sound is supported)");
  AudioFacts f;
  f.channels = b[pkt + 11];
  f.rate = (int)le32(b + pkt + 12);
  if (f.channels < 1 || f.rate < 1) return fail("the Ogg header has no sound format");
  f.ok = true;
  // The last page's granule position is the number of samples in the whole stream.
  const size_t lowest = n > 65536 ? n - 65536 : 0;
  for (size_t i = n >= 27 ? n - 27 : 0;; --i) {
    if (std::memcmp(b + i, "OggS", 4) == 0) {
      const uint64_t gp = le64(b + i + 6);
      if (gp != 0xFFFFFFFFFFFFFFFFull) f.seconds = (double)gp / (double)f.rate;
      break;
    }
    if (i <= lowest) break;
  }
  return f;
}

}  // namespace

AudioKind sniffAudio(const std::string& bytes) {
  if (bytes.size() >= 12 && bytes.compare(0, 4, "RIFF") == 0 && bytes.compare(8, 4, "WAVE") == 0) return AudioKind::Wav;
  if (bytes.size() >= 4 && bytes.compare(0, 4, "OggS") == 0) return AudioKind::Ogg;
  return AudioKind::Unknown;
}

AudioFacts readAudioFacts(const std::string& bytes, AudioKind kind) {
  switch (kind) {
    case AudioKind::Wav: return wavFacts(bytes);
    case AudioKind::Ogg: return oggFacts(bytes);
    default: return fail("not a sound file");
  }
}

}  // namespace ote_editor
