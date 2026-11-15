// --audio-test: plays the sound self-test (engine/player/sound/audio_selftest.h) through the real
// device and ends the player when it is over. Headless it only writes the --audio-log.
#pragma once
#include "scene.h"
#include "sound/audio_selftest.h"

namespace ote {

class AudioTestScene : public Scene {
 public:
  explicit AudioTestScene(SceneHost& host);
  void onInput(const InputEvent& e) override;
  void update(double dt) override;
  void draw(Renderer& r) override;

 private:
  SceneHost& host_;
  AudioSelfTest test_;
};

}  // namespace ote
