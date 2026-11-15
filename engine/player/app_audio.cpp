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
