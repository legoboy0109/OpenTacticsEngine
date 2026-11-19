#include "scene_options.h"

#include <cstring>
#include "sound/cue_sink.h"

#include "core/base/log.h"
#include "core/data/loader.h"

namespace ote {

OptionsScene::OptionsScene(SceneHost& host, const std::string& gameId) : host_(host), gameId_(gameId) {
  for (const GameEntry& g : host.games())
    if (g.id == gameId) entry_ = &g;
  if (perGame() && !entry_) gameId_.clear();  // unknown game: edit the global options instead

  OptionContext global;
  global.assetsDir = host.args().assetsDir;
  global.games = &host.games();
  OptionContext mine = global;
  mine.game = entry_ && perGame() ? entry_ : nullptr;

  for (const OptionDef& d : optionDefs()) {
    choices_.push_back(d.choices(mine));
    if (!perGame()) { globalLabel_.push_back(""); continue; }
    // What "Use global" means right now: the label of the global value, as this game lists it.
    std::vector<OptionChoice> gch = d.choices(global);
    const Json& gv = host.settings().global(d.key);
    std::string label = gch[0].label;
    if (!gv.isNull())
      for (const OptionChoice& c : choices_.back())
        if (!c.value.isNull() && sameValue(c.value, gv)) label = c.label;
    globalLabel_.push_back(label);
  }
  list_.count = defCount() + 2;

  if (perGame()) {  // terrain colours for the style swatches; failure only loses the swatches
    std::string err;
    dbLoaded_ = loadGame(host.gameDir(gameId_), db_, err);
    if (!dbLoaded_) OTE_WARN("options: no style preview for '%s': %s", gameId_.c_str(), err.c_str());
  }
}

Json OptionsScene::storedValue(int def) const {
  const char* key = optionDefs()[def].key;
  Settings& s = host_.settings();
  if (!perGame()) return s.global(key);
  return s.hasOverride(gameId_, key) ? s.get(gameId_, key) : Json();
}

bool OptionsScene::overridden(int def) const {
  return perGame() && host_.settings().hasOverride(gameId_, optionDefs()[def].key);
}

int OptionsScene::currentChoice(int def) {
  Json v = storedValue(def);
  std::vector<OptionChoice>& ch = choices_[def];
  for (size_t i = 0; i < ch.size(); ++i)
    if (sameValue(ch[i].value, v)) return (int)i;
  // A value that is no longer offered (font removed, hand-edited file): show it, do not hide it.
  OptionChoice c;
  c.label = (v.isString() ? v.asString() : v.dump(0)) + " (unavailable)";
  c.value = v;
  ch.push_back(c);
  return (int)ch.size() - 1;
}

std::string OptionsScene::choiceLabel(int def, int index) const {
  if (index == 0 && perGame()) return "Use global (" + globalLabel_[def] + ")";
  return choices_[def][index].label;
}

void OptionsScene::change(int def, int delta) {
  int n = (int)choices_[def].size();
  const OptionDef& od = optionDefs()[def];
  int next;
  if (od.slider) {
    // Element 0 is "unset"; the values follow in order. Start from the value in effect and stop at the ends.
    int cur = currentChoice(def);
    if (cur == 0) {
      Json g = perGame() ? host_.settings().global(od.key) : Json();
      cur = 1 + (g.isNumber() ? g.asInt(od.sliderDefault) : od.sliderDefault);
      if (cur < 1) cur = 1;
      if (cur > n - 1) cur = n - 1;
    } else if (cur + delta < 1 || cur + delta > n - 1) {
      return;  // already at the end: nothing changes and nothing is heard
    }
    next = cur + delta;
    if (next < 1) next = 1;
    if (next > n - 1) next = n - 1;
  } else {
    next = ((currentChoice(def) + delta) % n + n) % n;
  }
  const Json& v = choices_[def][next].value;
  const char* key = optionDefs()[def].key;
  if (perGame()) host_.settings().setOverride(gameId_, key, v);  // Null = back to following global
  else host_.settings().setGlobal(key, v);
  host_.relook(gameId_);
  // Moving a volume plays a short click at the new level, so the player can hear it.
  if (!strcmp(key, "musicVolume")) host_.audio().playTick(true);
  else if (!strcmp(key, "soundVolume")) host_.audio().playTick(false);
}

void OptionsScene::resetAll() {
  for (const OptionDef& d : optionDefs()) {
    if (perGame()) host_.settings().clearOverride(gameId_, d.key);
    else host_.settings().setGlobal(d.key, Json());
  }
  host_.relook(gameId_);
}

void OptionsScene::leave() {
  if (perGame()) host_.showGame(gameId_);
  else host_.showHome();
}

void OptionsScene::activate(int row) {
  uiCue(row == backRow() ? Cue::Cancel : Cue::Confirm);
  if (row < defCount()) change(row, 1);
  else if (row == resetRow()) resetAll();
  else leave();
}

void OptionsScene::onInput(const InputEvent& e) {
  layout(host_.renderer());
  if (e.type == InputEvent::PointerMove) {
    int row = list_.rowAt(e.x, e.y);
    if (row >= 0) list_.sel = row;
  } else if (e.type == InputEvent::PointerDown) {
    int row = list_.rowAt(e.x, e.y);
    if (row < 0) return;
    list_.sel = row;
    // On an option row the left arrow steps back, the rest of the row steps forward.
    if (row < defCount() && e.x >= valueX_ && e.x < valueX_ + arrowW_) change(row, -1);
    else activate(row);
  } else if (e.type == InputEvent::ButtonDown) {
    if (list_.step(e.button)) return;
    int row = list_.sel;
    if (e.button == Button::Left && row < defCount()) change(row, -1);
    else if (e.button == Button::Right && row < defCount()) change(row, 1);
    else if (e.button == Button::Confirm) activate(row);
    else if (e.button == Button::Cancel) { uiCue(Cue::Cancel); leave(); }
  }
}

}  // namespace ote
