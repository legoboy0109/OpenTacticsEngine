#include "scene_audio_test.h"

namespace ote {

AudioTestScene::AudioTestScene(SceneHost& host) : host_(host), test_(host.audio(), host.args().audioFile) {}

void AudioTestScene::onInput(const InputEvent& e) {
  if (e.type == InputEvent::ButtonDown && (e.button == Button::Cancel || e.button == Button::Menu)) host_.quit();
}

// Runs before AudioService::update (PlayerApp::update), so the clock it reads is this frame's start.
void AudioTestScene::update(double) {
  test_.update();
  if (test_.finished()) host_.quit();
}

void AudioTestScene::draw(Renderer& r) {
  r.clear(Color(20, 24, 32));
  const char* lines[] = {"Sound test", "", "You should hear a short tune with two chimes.", "It ends by itself after four seconds.",
                         "Press Cancel to stop it."};
  int y = 40;
  for (const char* l : lines) {
    r.drawText(30, y, l, Color(230, 230, 240), 1);
    y += r.lineHeight(1) + 4;
  }
}

}  // namespace ote
