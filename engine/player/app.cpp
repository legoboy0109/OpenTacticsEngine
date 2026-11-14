#include "views.h"
#include "app.h"

#include "battle/battle_scene.h"
#include "core/base/json.h"
#include "core/base/log.h"
#include "scene_game.h"
#include "scene_home.h"
#include "gfx3d/soft3d.h"
#include "platform/platform3d.h"
#include "scene_test3d.h"
#include "scene_options.h"
#include "scene_select.h"

namespace ote {

PlayerApp::PlayerApp(const Args& args) : args_(args) {}

// Last chance to keep changes when the window is closed or the process ends normally.
PlayerApp::~PlayerApp() { flushSettings(); }

// The software backend works on every platform; the platform's hardware
// backend takes its place where there is one (docs/RENDER3D.md). The choice
// is the "3D graphics" option (or --3d); left to the player, windowless runs
// keep the software one: their pictures must not depend on the machine's
// graphics driver.
Renderer3D& PlayerApp::renderer3d() {
  if (r3d_) return *r3d_;
  made3d_ = look_.draw3d;
  bool hardware = made3d_ == "opengl" || (made3d_.empty() && !args_.headless);
  if (hardware) {
    std::string why;
    r3d_ = createHardwareRenderer3D(why);
    if (!r3d_) OTE_WARN("3D drawing: no hardware backend (%s); using the software one", why.c_str());
  }
  if (!r3d_) {
    r3d_.reset(new SoftRenderer3D());
    OTE_INFO("3D drawing: software");
  }
  return *r3d_;
}

bool PlayerApp::start(std::string& err) {
  if (!args_.input.empty() && !script_.parse(args_.input, args_.inputInterval, err)) return false;
  return true;
}

namespace {
GameLook lookOf(const Database& db) {
  GameLook g;
  g.present = true;
  g.font.name = db.font.name;
  g.font.file = db.font.file;
  g.font.size = db.font.size;
  g.font.smooth = db.font.smooth;
  g.root = db.rootPath;
  for (const StyleDef& s : db.styles) {
    g.styleIds.push_back(s.id);
    g.styleUsable.push_back(viewSupported(s.view));
  }
  return g;
}

GameLook lookOf(const GameEntry& e, const std::string& gamesDir) {
  GameLook g;
  g.present = true;
  g.font.name = e.info.font.name;
  g.font.file = e.info.font.file;
  g.font.size = e.info.font.size;
  g.font.smooth = e.info.font.smooth;
  g.root = gamesDir + "/" + e.id;
  for (const StyleInfo& s : e.info.styles) {
    g.styleIds.push_back(s.id);
    g.styleUsable.push_back(viewSupported(s.view));
  }
  return g;
}

// Logical size of the screens while a game is loaded: the game's own view.
void viewSize(const Database& db, int& w, int& h) {
  w = 480;
  h = 270;
  const ViewConfig& v = db.view;
  if (v.width >= 160 && v.height >= 120 && v.width <= 1920 && v.height <= 1080) { w = v.width; h = v.height; }
}
}  // namespace

void PlayerApp::onStart(Renderer& r) {
  renderer_ = &r;
  loadSettings();
  std::string warning;
  games_ = discoverGames(args_.gamesDir, warning);
  if (notice_.empty()) notice_ = warning;
  findSavesFolder();
  if (args_.test3d) { applyLauncherLook(); scene_.reset(new Test3DScene(*this)); }
  else if (!args_.game.empty() && (args_.continueSave || args_.loadSlot > 0)) startFromSave();
  else if (!args_.game.empty()) doLoadGame(args_.game);
  else { applyLauncherLook(); showHome(); runPending(); }
}

void PlayerApp::runPending() {
  if (!pending_) return;
  std::function<void()> fn = std::move(pending_);
  pending_ = nullptr;
  fn();
  flushSettings();  // every scene switch is a good moment to save
}

void PlayerApp::onInput(const InputEvent& e) {
  if (e.type == InputEvent::Quit) { quit_ = true; flushSettings(); }
  else if (scene_) scene_->onInput(e);
  runPending();
}

void PlayerApp::update(double dt) {
  if (script_.active()) {
    std::vector<InputEvent> injected;
    script_.tick(injected);
    for (const InputEvent& ev : injected) onInput(ev);
    if (script_.finished() && args_.frames == 0) quit_ = true;
  }
  if (scene_) scene_->update(dt);
  runPending();
}

void PlayerApp::draw(Renderer& r) {
  if (scene_) scene_->draw(r);
  else r.clear(Color(0, 0, 0));
}

void PlayerApp::showHome() {
  later([this] {
    scene_.reset();  // the battle (if any) must die before the session changes
    dropSession();
    gameId_.clear();
    applyLauncherLook();  // back to the launcher's own font, scale and size
    scene_.reset(new HomeScene(*this));
  });
}

void PlayerApp::showGame(const std::string& gameId) {
  later([this, gameId] {
    const GameEntry* entry = nullptr;
    for (const GameEntry& g : games_)
      if (g.id == gameId) entry = &g;
    scene_.reset();
    dropSession();
    applyLauncherLook();
    if (!entry) { gameId_.clear(); scene_.reset(new HomeScene(*this)); return; }
    gameId_ = gameId;
    scene_.reset(new GameScene(*this, *entry));
  });
}

void PlayerApp::showOptions(const std::string& gameId) {
  later([this, gameId] {
    scene_.reset();
    dropSession();
    relook(gameId);
    scene_.reset(new OptionsScene(*this, gameId));
  });
}

void PlayerApp::relook(const std::string& gameId) {
  for (const GameEntry& g : games_)
    if (g.id == gameId && !gameId.empty()) { applyLook(gameId, lookOf(g, args_.gamesDir), 480, 270); return; }
  applyLauncherLook();
}

void PlayerApp::showMapSelect() {
  later([this] {
    if (!session_) { applyLauncherLook(); scene_.reset(new HomeScene(*this)); return; }
    scene_.reset(new MapSelectScene(*this));
  });
}

void PlayerApp::showError(const std::string& title, const std::string& message, Back back) {
  OTE_ERROR("%s: %s", title.c_str(), message.c_str());
  later([this, title, message, back] { scene_.reset(new ErrorScene(*this, title, message, back)); });
}

void PlayerApp::loadGame(const std::string& gameId) {
  later([this, gameId] { doLoadGame(gameId); });
}

void PlayerApp::startBattle(const std::string& mapId) {
  later([this, mapId] { doStartBattle(mapId); });
}

// Ends the current game: its art textures go first (the renderer is alive here).
void PlayerApp::dropSession() {
  if (renderer_) sprites_.release(*renderer_);
  if (renderer_) isoTiles_.release(*renderer_);
  if (renderer_) show3d_.release(*renderer_);
  session_.reset();
}

bool PlayerApp::loadSession(const std::string& gameId, std::string& err) {
  dropSession();
  session_.reset(new GameSession());
  if (!session_->load(args_.gamesDir + "/" + gameId, err)) {
    dropSession();
    return false;
  }
  OTE_INFO("loaded game '%s' (%s)", gameId.c_str(), session_->db.name.c_str());
  if (session_->db.graphics.any()) sprites_.load(*renderer_, session_->dir, session_->db.graphics);
  return true;
}

void PlayerApp::applyGameLook() {
  int w, h;
  viewSize(session_->db, w, h);
  applyLook(gameId_, lookOf(session_->db), w, h);
}

void PlayerApp::doLoadGame(const std::string& gameId) {
  scene_.reset();
  gameId_ = gameId;
  std::string err;
  if (!loadSession(gameId, err)) {
    applyLauncherLook();  // an error screen must not inherit the previous game's look
    showError("Cannot load game '" + gameId + "'", err, ToGame);
    runPending();
    return;
  }
  applyGameLook();
  bool direct = firstLoad_ && !args_.map.empty();
  firstLoad_ = false;
  if (direct) doStartBattle(args_.map);
  else { showMapSelect(); runPending(); }
}

void PlayerApp::doStartBattle(const std::string& mapId) {
  scene_.reset();
  std::string err;
  if (!session_->startMap(mapId, args_.seed, err)) {
    showError("Cannot start map '" + mapId + "'", err, ToMapSelect);
    runPending();
    return;
  }
  scene_.reset(new BattleScene(*this));
}

}  // namespace ote
