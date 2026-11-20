// The faction choice (docs/ECONOMY.md "Factions"): shown before a battle for each person-controlled side whose map says
// "faction": "choose". One row per faction; the selected one's description and what it brings are written under the list.
// Works with arrows + Confirm, a tap, and Cancel (back to map select). The battle starts after the last pick.
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "scene.h"
#include "ui/row_list.h"

namespace ote {

class FactionScene : public Scene {
 public:
  // `title` names the side that picks ("Blue Army"). onPick gets the faction's index in Database::factions; onCancel goes back.
  FactionScene(SceneHost& host, const std::string& title, std::function<void(int)> onPick, std::function<void()> onCancel);
  void onInput(const InputEvent& e) override;
  void draw(Renderer& r) override;
  SceneAudio audioContext() const override { SceneAudio a; a.kind = SceneAudio::Title; return a; }

  // What a faction brings, one line each ("Builds: Soldier, Archer", "Starts with: +50 Gold", ...); only what it says. Also in the tests' log.
  static std::vector<std::string> facts(const Database& db, const FactionDef& f);

 private:
  void layout(const Renderer& r);
  void activate(int row);

  SceneHost& host_;
  const std::vector<FactionDef>& list_;
  std::string title_;
  std::function<void(int)> onPick_;
  std::function<void()> onCancel_;
  RowList rows_;
};

}  // namespace ote
