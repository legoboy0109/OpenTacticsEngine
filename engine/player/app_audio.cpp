// Choosing the sound device at start-up (docs/SOUND.md "Engine design"). A machine without sound
// must still run the game, so every failure here is a warning, never an error.
#include "app.h"
#include "core/base/log.h"
#include "platform/platform.h"

namespace ote {

void PlayerApp::startAudio() {
  std::string err;
  if (!audio_.openLog(args_.audioLog, err)) OTE_WARN("audio: %s", err.c_str());
  if (args_.noAudio) {
    audio_.setMode(AudioService::Mode::Off);
    OTE_INFO("audio: off (--no-audio)");
  } else if (args_.headless) {
    audio_.setMode(AudioService::Mode::Silent);  // decodes and logs, plays nothing: tests must not open a device
  } else if (audioWaitsForUserInput()) {
    audio_.waitForUserInput();
  } else {
    openAudioDevice();
  }
}

// A scene appeared: make sure the open game's sound data is loaded (its page has no Database yet, the session
// has) and tell the music what the scene needs. Nothing is read in Off mode (docs/SOUND.md "Three modes").
void PlayerApp::syncAudio() {
  const unsigned id = scene_ ? scene_->audioId() : 0;
  if (id == audioScene_) return;
  audioScene_ = id;
  if (audio_.mode() == AudioService::Mode::Off) return;
  if (!launcherAudioTried_) {  // the player's own menu sounds, for the screens without a game
    launcherAudioTried_ = true;
    std::string err;
    if (!launcherAudio_.load(args_.assetsDir + "/launcher", "launcher", nullptr, err)) OTE_WARN("audio: launcher sounds: %s", err.c_str());
  }
  if (gameId_.empty()) {
    if (gameAudio_.loaded()) gameAudio_.unload();
  } else if (gameAudio_.gameId() != gameId_ || (session_ && !gameAudio_.full())) {
    std::string err;
    if (audioFailed_ != gameId_ + (session_ ? "+" : "") &&
        !gameAudio_.load(args_.gamesDir + "/" + gameId_, gameId_, session_ ? &session_->db : nullptr, err)) {
      OTE_WARN("audio: %s", err.c_str());
      audioFailed_ = gameId_ + (session_ ? "+" : "");
    }
  }
  if (scene_) gameAudio_.music().enter(scene_->audioContext());
}

void PlayerApp::openAudioDevice() {
  std::string why;
  std::unique_ptr<AudioOut> dev = openAudioOut(AudioService::kRate, why);
  if (!dev) {
    OTE_WARN("audio: %s; the game runs without sound", why.c_str());
    audio_.deviceFailed();
    return;
  }
  audio_.attachDevice(std::move(dev));
}

}  // namespace ote
