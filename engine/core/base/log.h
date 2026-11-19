// Minimal logging. The platform layer may install a sink (console, file,
// on-screen); the default writes to stderr.
#pragma once

// printf-style format checking where the compiler offers it (GCC/Clang); nothing on MSVC. Also used by the sim tool and
// the editor helper for their own printf-style functions.
#if defined(__GNUC__)
#define OTE_PRINTF_LIKE(fmtIndex, firstArg) __attribute__((format(printf, fmtIndex, firstArg)))
#else
#define OTE_PRINTF_LIKE(fmtIndex, firstArg)
#endif

namespace ote {

enum class LogLevel { Debug, Info, Warn, Error };
using LogSink = void (*)(LogLevel level, const char* message);

void setLogSink(LogSink sink);
void setLogLevel(LogLevel minimum);
LogLevel logLevel();  // the minimum shown now
void logf(LogLevel level, const char* fmt, ...) OTE_PRINTF_LIKE(2, 3);

}  // namespace ote

#define OTE_DEBUG(...) ::ote::logf(::ote::LogLevel::Debug, __VA_ARGS__)
#define OTE_INFO(...) ::ote::logf(::ote::LogLevel::Info, __VA_ARGS__)
#define OTE_WARN(...) ::ote::logf(::ote::LogLevel::Warn, __VA_ARGS__)
#define OTE_ERROR(...) ::ote::logf(::ote::LogLevel::Error, __VA_ARGS__)
