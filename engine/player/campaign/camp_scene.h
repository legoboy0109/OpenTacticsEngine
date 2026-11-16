// The camp: the screen between two chapters that lists the support conversations waiting to be played
// (docs/SRPG.md "Player UI > Camp", step U5). Title "Camp", one row per pending pair ("Eirik and Lyra", with the
// level it unlocks), and "Continue the story" last. Choosing a pair plays its lines in a StoryScene (the same
// dialogue box as every scene), which saves the playthrough and brings the camp back with the pair gone from the
// list; "Continue the story" goes on to the chapter's title card and before scene.
//
// The camp is not a phase of the playthrough: campStep() decides, each time the flow reaches phase Before of a
// chapter other than the first, whether to show it. --autoplay never does.
#pragma once
#include <memory>
#include <vector>

#include "battle/support_text.h"
#include "scene.h"
#include "ui/row_list.h"

namespace ote {

class CampScene : public Scene {
 public:
  CampScene(SceneHost& host, std::vector<CampRow> rows);
  void onInput(const InputEvent& e) override;
  void draw(Renderer& r) override;
  SceneAudio audioContext() const override { SceneAudio a; a.kind = SceneAudio::Scene; return a; }

 private:
  void layout(const Renderer& r);
  void choose(int row);  // row == rows_.size() is "Continue the story"
  SceneHost& host_;
  std::vector<CampRow> rows_;
  Color bg_;
  RowList list_;
  bool left_ = false;
};

// What the flow shows at phase Before before the chapter's own scene: the camp, or the conversation just chosen in
// it, or null = go on to the before scene. Needs a campaign session. Logs what it decides.
std::unique_ptr<Scene> campStep(SceneHost& host);

}  // namespace ote
