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
  readIndex();
  if (!args_.game.empty()) doLoadGame(args_.game);
  else if (games_.size() == 1) doLoadGame(games_[0].id);
  else if (!indexError_.empty())
    showError("Cannot read game list", indexError_, ToGameSelect), runPending();
  else showGameSelect(), runPending();
  return true;
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
    std::vector<GameEntry> games = games_;
    scene_.reset(new GameSelectScene(*this, games, args_.gamesDir));
  });
}

void PlayerApp::showMapSelect() {
  later([this] {
    if (!session_) { scene_.reset(new GameSelectScene(*this, games_, args_.gamesDir)); return; }
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
  session_.reset(new GameSession());
  std::string err;
  if (!session_->load(args_.gamesDir + "/" + gameId, err)) {
    session_.reset();
    showError("Cannot load game '" + gameId + "'", err, ToGameSelect);
    runPending();
    return;
  }
  OTE_INFO("loaded game '%s' (%s)", gameId.c_str(), session_->db.name.c_str());
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
