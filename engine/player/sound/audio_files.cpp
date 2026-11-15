#include "sound/audio_files.h"

#include <cstdio>

namespace ote {

FileBytes readAudioFile(const std::string& path, std::string& err) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) {
    err = "cannot read " + path;
    return nullptr;
  }
  std::vector<uint8_t>* bytes = new std::vector<uint8_t>();
  uint8_t buf[65536];
  size_t n;
  const size_t kMax = (size_t)256 * 1024 * 1024;  // a runaway file must not eat all memory
  bool tooBig = false;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) {
    bytes->insert(bytes->end(), buf, buf + n);
    if (bytes->size() > kMax) { tooBig = true; break; }
  }
  std::fclose(f);
  if (tooBig) {
    err = path + " is too large for a sound file";
    delete bytes;
    return nullptr;
  }
  return FileBytes(bytes);
}

}  // namespace ote
