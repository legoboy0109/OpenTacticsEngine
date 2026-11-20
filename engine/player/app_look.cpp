// Settings file handling and applying the effective look.
#include "app.h"
#include "core/base/log.h"
#include "options/option_defs.h"
#include "platform/platform.h"

namespace ote {

void PlayerApp::loadSettings() {
  settingsPath_ = args_.noSettings ? "" : args_.settings;
  if (settingsPath_.empty() && !args_.headless && !args_.noSettings) {
    // Headless runs are tests: never read or write the user's real settings unless asked to.
    std::string dir = userRoot();
    if (dir.empty()) OTE_WARN("no user data folder: options will not be remembered");
    else settingsPath_ = dir + "/settings.json";
  }
  if (settingsPath_.empty()) return;
  std::string err;
  if (!settings_.load(settingsPath_, err)) {
    OTE_ERROR("%s (using defaults; the file is kept as .bad when settings are next saved)", err.c_str());
    notice_ = "Settings file unreadable - using defaults";
  } else {
    OTE_INFO("settings: %s", settingsPath_.c_str());
  }
}

void PlayerApp::flushSettings() {
  if (settingsPath_.empty() || !settings_.dirty()) return;
  std::string err;
  if (settings_.save(err)) {
    commitUserData();  // web: push the written file into browser storage
    return;
  }
  if (!saveWarned_) OTE_ERROR("cannot save settings: %s", err.c_str());
  saveWarned_ = true;
}

namespace {
LookLayer argsLayer(const Args& a) {
  LookLayer l;
  l.font = a.font;
  l.fontSize = a.fontSize;
  l.smooth = a.fontSmooth;
  l.uiScale = a.uiScale;
  l.style = a.style;
  l.draw3d = a.draw3d == "hardware" ? "opengl" : a.draw3d == "software" ? "software" : "";
  l.battleScene = a.battleScene;
  l.musicVolume = a.musicVolume;
  l.soundVolume = a.soundVolume;
  l.res3d = a.res3d == "display" || a.res3d == "game" ? a.res3d : "";
  // A headless run asked to draw 3D bigger has no display to follow: that is the point of --3d-scale.
  if (l.res3d.empty() && a.headless && a.scale3d > 1) l.res3d = "display";
  return l;
}
}  // namespace

void PlayerApp::applyLook(const std::string& gameId, const GameLook& game, int w, int h) {
  LookLayer perGame = gameId.empty() ? LookLayer() : overrideLayer(settings_, gameId);
  look_ = resolveLook(args_.assetsDir, argsLayer(args_), perGame, globalLayer(settings_), game);
  renderer_->setLogicalSize(w, h);
  renderer_->setSafeArea(look_.safeArea);  // only the original Xbox backend acts on it
  show3d_.setResolution(look_.res3d);
  audio_.setVolumes(look_.musicVolume, look_.soundVolume);
  // Another 3D drawer was chosen: the next user of the 3D layer gets a new
  // one. Nothing holds 3D things here (looks are applied between scenes and
  // on the options screen, which have none).
  if (r3d_ && look_.draw3d != made3d_) {
    show3d_.release(*renderer_);
    r3d_.reset();
  }
  FontRequest req = look_.font;
  if (fontApplied_ && req.file == appliedFont_.file && req.size == appliedFont_.size &&
      req.smooth == appliedFont_.smooth)
    return;  // unchanged: skip re-reading the font file
  if (!renderer_->setFont(req)) {  // unusable file: the engine already fell back to the built-in font
    req = pixelFontRequest();
    renderer_->setFont(req);
  }
  appliedFont_ = req;
  fontApplied_ = true;
  OTE_INFO("font: %s size %d %s (ui scale %d%%)", req.file.empty() ? "built-in pixel" : req.file.c_str(), req.size,
           req.smooth ? "smooth" : "crisp", look_.uiScale);
}

}  // namespace ote
