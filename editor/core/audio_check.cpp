#include "audio_check.h"

#include "sound/game_audio.h"

namespace ote_editor {

std::string checkAudio(const std::string& root, const ote::Database& db) {
  // Silent mode is what a headless player run uses: effects are really decoded (so damaged or over-long files are
  // found) and nothing is mixed or played; no device is opened.
  ote::AudioService service;
  service.setMode(ote::AudioService::Mode::Silent);
  ote::GameAudio game(service);
  std::string err;
  if (!game.load(root, "check", &db, err)) return err.empty() ? "the sound settings could not be loaded" : err;
  return "";
}

}  // namespace ote_editor
