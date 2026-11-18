// Minimal logging. The platform layer may install a sink (console, file,
// on-screen); the default writes to stderr.
#pragma once

namespace ote {

enum class LogLevel { Debug, Info, Warn, Error };
using LogSink = void (*)(LogLevel level, const char* message);

void setLogSink(LogSink sink);
void setLogLevel(LogLevel minimum);
LogLevel logLevel();  // the minimum shown now
#if defined(__GNUC__)
void logf(LogLevel level, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
#else
void logf(LogLevel level, const char* fmt, ...);
#endif

}  // namespace ote

#define OTE_DEBUG(...) ::ote::logf(::ote::LogLevel::Debug, __VA_ARGS__)
#define OTE_INFO(...) ::ote::logf(::ote::LogLevel::Info, __VA_ARGS__)
#define OTE_WARN(...) ::ote::logf(::ote::LogLevel::Warn, __VA_ARGS__)
#define OTE_ERROR(...) ::ote::logf(::ote::LogLevel::Error, __VA_ARGS__)
