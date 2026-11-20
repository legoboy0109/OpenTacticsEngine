// Runs the PLAYER's own sound loader over a project (docs/SOUND.md "Where ids are checked"), so /problems tells what
// the player would refuse: a wrong data/audio.json, an unknown sound id in a weapon, unit, item, map or story command,
// a sound file that is damaged or longer than 10 seconds. A sound file that is merely missing is only a warning in
// the player (the game starts without that sound) and is not reported here either: the browser editor shows it.
#pragma once
#include <string>

#include "core/data/database.h"

namespace ote_editor {

// Empty when the sound data is fine (or the game has none); otherwise the loader's message, which starts with the
// project path like every other loader message.
std::string checkAudio(const std::string& root, const ote::Database& db);

}  // namespace ote_editor
