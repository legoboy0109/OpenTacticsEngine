// Scene presentation commands in a battle (docs/EVENTS.md "Scene presentation"). Everything here is presentation: the
// rules never read it. A command leaves a SceneEffect entry in the event stream (the player plays them in order, so the
// commands behind a fade run in order behind it) and the few facts a resumed battle needs (the background, the weather)
// are kept in StoryState and saved.
#include <algorithm>
#include <queue>

#include "core/battle/battle.h"

namespace ote {

namespace {

Event sceneEvent(SceneFx::Kind kind, int text, int ms, int d, int flags) {
  Event e;
  e.type = EventType::SceneEffect;
  e.a = (int)kind;
  e.b = text;
  e.c = ms;
  e.d = d;
  e.flags = flags;
  return e;
}

}  // namespace

void Battle::storyScene(const Command& c, std::vector<Event>& events) {
  using K = SceneFx::Kind;
  const SceneFx& fx = c.fx;
  switch (fx.kind) {
    case K::Background:
      storyState.background = fx.text >= 0 ? storyText(fx.text) : std::string();
      events.push_back(sceneEvent(K::Background, fx.text, fx.ms, 0, fx.fadeTransition ? kSceneFadeTrans : 0));
      break;
    case K::Fade:
      storyState.faded = fx.out;
      if (fx.out) storyState.fadeColor = fx.color;
      events.push_back(sceneEvent(K::Fade, -1, fx.ms, (int)fx.color, fx.out ? kSceneOut : 0));
      break;
    case K::Wait: events.push_back(sceneEvent(K::Wait, -1, fx.ms, 0, 0)); break;
    case K::Shake: events.push_back(sceneEvent(K::Shake, -1, fx.ms, fx.strength, 0)); break;
    case K::Leave: events.push_back(sceneEvent(K::Leave, fx.text, -1, 0, 0)); break;
    case K::Weather:
      storyState.weather = storyText(fx.text) == "none" ? std::string() : storyText(fx.text);
      events.push_back(sceneEvent(K::Weather, fx.text, -1, 0, 0));
      break;
    case K::None: break;
  }
}

// A fade belongs to the run of events that started it: when the run is over and the screen is still faded out, the core
// fades it in (so a map event that forgot `fade in` never leaves the player with a black screen, and no save is ever
// made while faded).
void Battle::endStoryRun(std::vector<Event>& events) {
  if (choice_.halted || !storyState.faded) return;
  events.push_back(sceneEvent(SceneFx::Kind::Fade, -1, kDefaultFadeMs, (int)storyState.fadeColor, kSceneAuto));
  storyState.faded = false;
}

// The way a walked moveUnit takes: the cheapest path over the terrain for the unit's movement type, with no movement
// budget and with units ignored (a story move walks through anyone; the rules result is the same relocation as without
// "walk"). Ties go to the lower cell number, so it is the same on every machine. False when the square cannot be reached.
bool Battle::storyWalkPath(int unit, Cell from, Cell to, std::vector<int>& path) const {
  path.clear();
  if (unit < 0 || unit >= (int)units.size() || !grid.valid(from) || !grid.valid(to)) return false;
  const int moveType = typeOf(units[(size_t)unit]).moveType;
  const int n = grid.cellCount();
  std::vector<int> dist((size_t)n, -1);
  std::vector<Cell> parent((size_t)n, kNoCell);
  typedef std::pair<int, Cell> Node;
  std::priority_queue<Node, std::vector<Node>, std::greater<Node> > open;
  dist[(size_t)from] = 0;
  open.push(Node(0, from));
  while (!open.empty()) {
    const Node node = open.top();
    open.pop();
    if (node.first != dist[(size_t)node.second]) continue;
    if (node.second == to) break;
    Cell nb[Grid::kMaxNeighbors];
    const int count = grid.neighbors(node.second, nb);
    for (int i = 0; i < count; ++i) {
      const int step = enterCost(nb[i], moveType);
      if (step <= 0) continue;
      const int total = node.first + step;
      if (dist[(size_t)nb[i]] != -1 && dist[(size_t)nb[i]] <= total) continue;
      dist[(size_t)nb[i]] = total;
      parent[(size_t)nb[i]] = node.second;
      open.push(Node(total, nb[i]));
    }
  }
  if (dist[(size_t)to] < 0) return false;
  for (Cell c = to; c != kNoCell; c = parent[(size_t)c]) path.push_back((int)c);
  std::reverse(path.begin(), path.end());
  return path.size() >= 2;
}

}  // namespace ote
