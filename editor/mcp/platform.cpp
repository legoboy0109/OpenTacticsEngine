#include "platform.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace ote_mcp {

#if defined(_WIN32)

void setBinaryStdio() {
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
}

std::string executableDir(const char* argv0) {
  char buf[MAX_PATH * 4];
  const DWORD n = GetModuleFileNameA(nullptr, buf, (DWORD)sizeof buf);
  std::string p = n > 0 && n < sizeof buf ? std::string(buf, n) : std::string(argv0 ? argv0 : "");
  const size_t slash = p.find_last_of("\\/");
  return slash == std::string::npos ? std::string() : p.substr(0, slash);
}

// Quotes one argument for CreateProcess' single command line (the usual backslash and quote rules of the MSVC runtime).
static std::string quoteArg(const std::string& a) {
  if (!a.empty() && a.find_first_of(" \t\n\v\"") == std::string::npos) return a;
  std::string out = "\"";
  size_t backslashes = 0;
  for (char c : a) {
    if (c == '\\') { ++backslashes; continue; }
    if (c == '"') { out.append(backslashes * 2 + 1, '\\'); out += '"'; }
    else { out.append(backslashes, '\\'); out += c; }
    backslashes = 0;
  }
  out.append(backslashes * 2, '\\');
  return out + "\"";
}

ProcessResult runProcess(const std::vector<std::string>& argv, const std::vector<std::string>& env, int timeoutMs, size_t maxOutput) {
  ProcessResult r;
  for (const std::string& e : env) {
    const size_t eq = e.find('=');
    if (eq != std::string::npos) SetEnvironmentVariableA(e.substr(0, eq).c_str(), e.substr(eq + 1).c_str());
  }
  SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
  HANDLE rd = nullptr, wr = nullptr;
  if (!CreatePipe(&rd, &wr, &sa, 0)) { r.error = "cannot create a pipe"; return r; }
  SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
  STARTUPINFOA si{};
  si.cb = sizeof si;
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = wr;
  si.hStdError = wr;
  si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  PROCESS_INFORMATION pi{};
  std::string cmd;
  for (const std::string& a : argv) cmd += (cmd.empty() ? "" : " ") + quoteArg(a);
  if (!CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    r.error = "cannot start " + argv[0] + " (error " + std::to_string(GetLastError()) + ")";
    CloseHandle(rd);
    CloseHandle(wr);
    return r;
  }
  CloseHandle(wr);
  r.started = true;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  for (;;) {
    DWORD avail = 0;
    if (PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
      char buf[4096];
      DWORD got = 0;
      if (ReadFile(rd, buf, sizeof buf, &got, nullptr) && got > 0 && r.output.size() < maxOutput) r.output.append(buf, got);
      continue;
    }
    if (WaitForSingleObject(pi.hProcess, 20) == WAIT_OBJECT_0) break;
    if (std::chrono::steady_clock::now() > deadline) {
      TerminateProcess(pi.hProcess, 1);
      r.timedOut = true;
      WaitForSingleObject(pi.hProcess, 2000);
      break;
    }
  }
  DWORD code = 0;
  GetExitCodeProcess(pi.hProcess, &code);
  r.exitCode = (int)code;
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  CloseHandle(rd);
  return r;
}

#else  // POSIX

void setBinaryStdio() {
  signal(SIGPIPE, SIG_IGN);  // a client that went away is a failed write, not a dead process
}

std::string executableDir(const char* argv0) {
  std::string p;
  char buf[4096];
  const ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);  // Linux; other systems fall back to argv[0]
  if (n > 0) p.assign(buf, (size_t)n);
  else if (argv0) p = argv0;
  const size_t slash = p.find_last_of('/');
  return slash == std::string::npos ? std::string() : p.substr(0, slash);
}

ProcessResult runProcess(const std::vector<std::string>& argv, const std::vector<std::string>& env, int timeoutMs, size_t maxOutput) {
  ProcessResult r;
  int fds[2];
  if (pipe(fds) != 0) { r.error = "cannot create a pipe"; return r; }
  std::vector<char*> args;
  for (const std::string& a : argv) args.push_back(const_cast<char*>(a.c_str()));
  args.push_back(nullptr);
  std::vector<std::string> envStore;
  std::vector<char*> envp;
  for (char** e = environ; e && *e; ++e) envp.push_back(*e);
  for (const std::string& e : env) envStore.push_back(e);
  for (std::string& e : envStore) envp.push_back(&e[0]);
  envp.push_back(nullptr);
  const pid_t pid = fork();
  if (pid < 0) { close(fds[0]); close(fds[1]); r.error = "cannot start a process"; return r; }
  if (pid == 0) {
    dup2(fds[1], 1);
    dup2(fds[1], 2);
    close(fds[0]);
    close(fds[1]);
    const int devnull = open("/dev/null", O_RDONLY);
    if (devnull >= 0) dup2(devnull, 0);
    execve(args[0], args.data(), envp.data());  // execve: the program path is given in full, no PATH search, no shell
    _exit(127);
  }
  close(fds[1]);
  r.started = true;
  fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL, 0) | O_NONBLOCK);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  bool eof = false;
  int status = 0;
  bool exited = false;
  while (!exited) {
    pollfd p{fds[0], POLLIN, 0};
    if (!eof) {
      poll(&p, 1, 50);
      char buf[4096];
      for (;;) {
        const ssize_t got = read(fds[0], buf, sizeof buf);
        if (got > 0) { if (r.output.size() < maxOutput) r.output.append(buf, (size_t)got); }
        else { if (got == 0) eof = true; break; }
      }
    } else {
      usleep(20000);
    }
    if (waitpid(pid, &status, WNOHANG) == pid) exited = true;
    else if (std::chrono::steady_clock::now() > deadline) {
      kill(pid, SIGKILL);  // only this child, by its pid
      waitpid(pid, &status, 0);
      r.timedOut = true;
      exited = true;
    }
  }
  // Whatever the child wrote last.
  char buf[4096];
  for (;;) {
    const ssize_t got = read(fds[0], buf, sizeof buf);
    if (got <= 0) break;
    if (r.output.size() < maxOutput) r.output.append(buf, (size_t)got);
  }
  close(fds[0]);
  r.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  return r;
}

#endif

}  // namespace ote_mcp
