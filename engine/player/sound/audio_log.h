// The audio log: one line per sound / music request, so tests (and later the game's own tests) can
// assert what would be heard without listening (docs/SOUND.md "Audio log").
#pragma once
#include <cstdio>
#include <string>
#include <vector>

namespace ote {

class AudioLog {
 public:
  ~AudioLog() { close(); }
  // Opens `path` for writing (empty = no file). False and a message when it cannot be written.
  bool open(const std::string& path, std::string& err);
  void close();
  // Keep the lines in memory too (tests).
  void keepLines(bool keep) { keep_ = keep; }
  const std::vector<std::string>& lines() const { return lines_; }
  // "t=<seconds> <text>"; seconds with three decimals, so a --fixed-dt run writes the same file every time.
  void write(double seconds, const std::string& text);

 private:
  FILE* file_ = nullptr;
  bool keep_ = false;
  std::vector<std::string> lines_;
};

}  // namespace ote
