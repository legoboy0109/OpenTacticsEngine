// The one-line way for a widget or scene to make a menu sound: `uiCue(Cue::Confirm);` (docs/SOUND.md).
// The panels in ui/ know nothing about the App, the game or the sound files; the App installs a sink
// (GameAudioCues, game_audio.h) that plays the cue with the open game's sounds. With no sink installed
// (tests, a game without sound) the call does nothing. Header-only so the ui/ sources need no extra files.
#pragma once
#include "sound/cues.h"

namespace ote {

class CueSink {
 public:
  virtual ~CueSink() {}
  virtual void onCue(Cue cue) = 0;
};

inline CueSink*& cueSinkSlot() {
  static CueSink* sink = nullptr;
  return sink;
}

inline void uiCue(Cue cue) {
  if (CueSink* s = cueSinkSlot()) s->onCue(cue);
}

}  // namespace ote
