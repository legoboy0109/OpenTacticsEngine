#include "sound/audio_log.h"

#include "core/base/fileio.h"

namespace ote {

bool AudioLog::open(const std::string& path, std::string& err) {
  close();
  if (path.empty()) return true;
  file_ = openFile(path, "wb");
  if (!file_) {
    err = "cannot write the audio log " + path;
    return false;
  }
  return true;
}

void AudioLog::close() {
  if (file_) std::fclose(file_);
  file_ = nullptr;
}

void AudioLog::write(double seconds, const std::string& text) {
  char head[48];
  std::snprintf(head, sizeof head, "t=%.3f ", seconds);
  std::string line = std::string(head) + text;
  if (keep_) lines_.push_back(line);
  if (file_) {
    std::fputs(line.c_str(), file_);
    std::fputc('\n', file_);
    std::fflush(file_);  // a crash or a killed run must not lose the lines a test wants to read
  }
}

}  // namespace ote
