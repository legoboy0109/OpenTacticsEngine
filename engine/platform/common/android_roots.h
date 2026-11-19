// Android-only helpers behind contentRoot() / extraContentRoot() (engine/platform/common/roots.cpp) and the log sink.
// Implemented in platform/sdl3/sdl_android.cpp (SDL3 owns the JNI glue); the declarations compile everywhere but the
// definitions exist only when __ANDROID__ is set.
#pragma once
#include <string>

namespace ote {

// Folder the APK's assets (games/, assets/) were extracted to; extracts them first when the app was installed or
// updated since the last run (see docs/ANDROID.md). Empty if extraction failed. Runs once per process.
std::string androidContentRoot();
// App-specific external storage "<...>/Android/data/<id>/files/games" (created); users drop games there without any
// permission. Empty if external storage is unavailable.
std::string androidExtraGamesRoot();
// Routes ote::logf to logcat (tag "OTE"); stderr goes nowhere on Android.
void installAndroidLogSink();

}  // namespace ote
