// Suspend / resume / low memory / terminate (platform.h Lifecycle; docs/PORTING.md "Life cycle").
//
// A suspended mobile or console app can be killed without another word, so going to the background does what leaving
// the player right now would do: the settings are written, and a battle in play writes its Continue save (the same
// mechanism as "Leave battle", see BattleScene::onSuspend; there is no separate suspend file). Both respect the run's
// switches: with --no-settings / --no-saving (or a headless run without --settings / --saves) there is no file to write
// to, so nothing is written. The simulation clock and the sound stand still until the app is in the foreground again.
//
// Not handled (and why): the renderer. Neither backend has a device-reset path (no SDL_EVENT_RENDER_DEVICE_RESET /
// SDL_RENDERTARGETSRESET handling): textures made through Renderer::createTexture are assumed to survive. Watch for
// this on Android (GL context loss) and UWP (D3D device removal).
#include "app.h"
#include "core/base/log.h"

namespace ote {

void PlayerApp::onLifecycle(Lifecycle event) {
  switch (event) {
    case Lifecycle::WillEnterBackground:
    case Lifecycle::Terminating:
      OTE_INFO("lifecycle: %s", event == Lifecycle::Terminating ? "terminating" : "background");
      suspended_ = true;
      audio_.suspend();
      if (settingsPending()) OTE_INFO("lifecycle: settings written");
      flushSettings();
      if (scene_) scene_->onSuspend();
      break;
    case Lifecycle::DidEnterForeground:
      OTE_INFO("lifecycle: foreground");
      suspended_ = false;
      audio_.resume();
      break;
    case Lifecycle::LowMemory:
      // Nothing of ours is safe to drop and cheap to rebuild yet (textures and the loaded game are in use); the settings are
      // written in case the OS follows this with a kill.
      OTE_INFO("lifecycle: low memory");
      if (settingsPending()) OTE_INFO("lifecycle: settings written");
      flushSettings();
      break;
  }
}

}  // namespace ote
