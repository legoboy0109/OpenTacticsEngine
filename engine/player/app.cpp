#include "app.h"

#include "battle/battle_scene.h"
#include "core/base/json.h"
#include "core/base/log.h"

namespace ote {

PlayerApp::PlayerApp(const Args& args) : args_(args) {}

void PlayerApp::readIndex() {
  games_.clear();
  Json index;
  std::string err;
  if (!loadJsonFile(args_.gamesDir + "/index.json", index, err)) {
    indexError_ = err;
    OTE_WARN("%s", err.c_str());
    return;
  }
  for (const Json& g : index["games"].items()) {
    GameEntry e;
    e.id = g.asString();
    if (e.id.empty()) continue;
    e.name = e.id;
    Json gj;
    std::string gerr;
    if (loadJsonFile(args_.gamesDir + "/" + e.id + "/game.json", gj, gerr)) e.name = gj["name"].asString(e.id);
    else { e.readable = false; OTE_WARN("%s", gerr.c_str()); }
    games_.push_back(e);
  }
}

bool PlayerApp::start(std::string& err) {
  if (!args_.input.empty() && !script_.parse(args_.input, args_.inputInterval, err)) return false;
  return true;
}

namespace {
constexpr int kDefaultW = 480, kDefaultH = 270;  // logical size of the list screens

bool isBuiltinName(const std::string& s) { return s.find_first_of("/\\.") == std::string::npos; }
}  // namespace

// Precedence: command line > the loaded game's "font" > "pixel". --font
// replaces the game's font entirely (its size/smooth defaults come with it);
// --font-size / --font-smooth then override single fields of either.
void PlayerApp::applyScreen(const Database* game) {
  int w = kDefaultW, h = kDefaultH;
  FontChoice choice;
  std::string root;
  if (game) {
    const ViewConfig& v = game->view;
    if (v.width >= 160 && v.height >= 120 && v.width <= 1920 && v.height <= 1080) { w = v.width; h = v.height; }
    choice.name = game->font.name;
    choice.file = game->font.file;
    choice.size = game->font.size;
    choice.smooth = game->font.smooth;
    root = game->rootPath;
  }
  if (!args_.font.empty()) {
    choice = FontChoice();
    root.clear();
    if (isBuiltinName(args_.font)) choice.name = args_.font;
    else choice.file = args_.font;
  }
  if (args_.fontSize > 0) choice.size = args_.fontSize;
  if (args_.fontSmooth >= 0) choice.smooth = args_.fontSmooth;

  renderer_->setLogicalSize(w, h);
  FontRequest req = resolveFont(args_.assetsDir, root, choice);
  if (fontApplied_ && req.file == appliedFont_.file && req.size == appliedFont_.size &&
      req.smooth == appliedFont_.smooth)
    return;  // unchanged: skip re-reading the font file
  if (!renderer_->setFont(req)) renderer_->setFont(pixelFontRequest());
  appliedFont_ = req;
  fontApplied_ = true;
  OTE_INFO("font: %s size %d %s", req.file.empty() ? "built-in pixel" : req.file.c_str(), req.size,
           req.smooth ? "smooth" : "crisp");
}

void PlayerApp::onStart(Renderer& r) {
  renderer_ = &r;
  applyScreen(nullptr);
  readIndex();
  if (!args_.game.empty()) doLoadGame(args_.game);
  else if (games_.size() == 1) doLoadGame(games_[0].id);
  else if (!indexError_.empty())
    showError("Cannot read game list", indexError_, ToGameSelect), runPending();
  else showGameSelect(), runPending();
}

void PlayerApp::runPending() {
  if (!pending_) return;
  std::function<void()> fn = std::move(pending_);
  pending_ = nullptr;
  fn();
}

void PlayerApp::onInput(const InputEvent& e) {
  if (e.type == InputEvent::Quit) quit_ = true;
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

void PlayerApp::showGameSelect() {
  later([this] {
    scene_.reset();  // the battle (if any) must die before the session changes
    applyScreen(nullptr);  // lists use the default size and font
    std::vector<GameEntry> games = games_;
    scene_.reset(new GameSelectScene(*this, games, args_.gamesDir));
  });
}

void PlayerApp::showMapSelect() {
  later([this] {
    if (!session_) { applyScreen(nullptr); scene_.reset(new GameSelectScene(*this, games_, args_.gamesDir)); return; }
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

void PlayerApp::doLoadGame(const std::string& gameId) {
  scene_.reset();
  applyScreen(nullptr);  // an error screen must not inherit the previous game's look
  session_.reset(new GameSession());
  std::string err;
  if (!session_->load(args_.gamesDir + "/" + gameId, err)) {
    session_.reset();
    showError("Cannot load game '" + gameId + "'", err, ToGameSelect);
    runPending();
    return;
  }
  OTE_INFO("loaded game '%s' (%s)", gameId.c_str(), session_->db.name.c_str());
  applyScreen(&session_->db);
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
