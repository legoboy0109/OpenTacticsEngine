// The only place of ote-mcp with operating-system calls: binary stdio, finding the program's own folder and running a child
// process (the headless player and the simulator). POSIX and Windows branches live in platform.cpp. The Windows branch has
// never been compiled or run (no Windows machine at hand when it was written): treat it as unverified.
#pragma once
#include <string>
#include <vector>

namespace ote_mcp {

// Puts stdin/stdout into binary mode where the C library would otherwise translate line ends (Windows); no-op elsewhere.
void setBinaryStdio();

// Folder that holds the running program ("" when unknown). `argv0` is the fallback.
std::string executableDir(const char* argv0);

struct ProcessResult {
  bool started = false;   // false: the program could not be launched (`error` says why)
  bool timedOut = false;  // killed because it ran longer than allowed
  int exitCode = -1;
  std::string output;     // stdout and stderr together, cut at `maxOutput` bytes
  std::string error;
};

// Runs `argv[0]` with the arguments, no shell involved. `env` entries ("NAME=value") are added to the environment.
// Waits at most `timeoutMs`; a child that is still running then is terminated (only that child).
ProcessResult runProcess(const std::vector<std::string>& argv, const std::vector<std::string>& env, int timeoutMs,
                         size_t maxOutput = 1 << 20);

}  // namespace ote_mcp
