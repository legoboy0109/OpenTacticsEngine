#include "scene_select.h"

namespace ote {

static std::vector<ListEntry> gameEntries(const std::vector<GameEntry>& games) {
  std::vector<ListEntry> out;
  for (const GameEntry& g : games) {
    ListEntry e;
    e.label = g.name;
    e.detail = g.readable ? g.id : g.id + " (cannot read game.json)";
    out.push_back(e);
  }
  return out;
}

GameSelectScene::GameSelectScene(SceneHost& host, const std::vector<GameEntry>& games,
                                 const std::string& dir)
    : ListScene(host, "Select a game", gameEntries(games), "Up/Down + Confirm, or tap  |  folder: " + dir) {
  for (const GameEntry& g : games) ids_.push_back(g.id);
}

static std::vector<ListEntry> mapEntries(GameSession* s) {
  std::vector<ListEntry> out;
  for (const MapRef& m : s->db.maps) {
    ListEntry e;
    e.label = m.name.empty() ? m.id : m.name;
    e.detail = m.id;
    out.push_back(e);
  }
  return out;
}

MapSelectScene::MapSelectScene(SceneHost& host)
    : ListScene(host, host.session()->db.name, mapEntries(host.session()),
                "Select a map  |  Cancel: back to games"),
      session_(host.session()) {}

}  // namespace ote
