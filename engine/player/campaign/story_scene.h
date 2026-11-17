// A campaign scene on its own screen (docs/EVENTS.md "Campaign"): the style's background colour, an
// optional title card ("Chapter N" over the chapter's name) and then the scene's lines in the same
// DialogueBox a battle uses, with the same controls: Confirm / tap = finish the page, turn it, end the
// line; holding Cancel or Menu, the Skip tab or the `skip` script token = skip the rest of the scene.
// In --autoplay the card and every line pass by themselves so smoke tests never hang on a scene.
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "core/campaign/playthrough.h"
#include "scene.h"
#include "ui/dialogue_box.h"
#include "ui/scene_stage.h"
#include "ui/scene_stage_draw.h"

namespace ote {

// Runs the scene again from before it started with the picks made so far (dialogue choices, runScene with answers) and
// returns all its lines; false = it could not. Null = the scene has no questions to ask.
using SceneResolver = std::function<bool(const std::vector<int>& answers, std::vector<SceneLine>& lines)>;

class StoryScene : public Scene {
 public:
  // `heading` / `title` = the card ("Chapter 2" / the chapter's name); both empty = no card.
  // At least one of card and lines is expected (an empty scene is not shown at all).
  StoryScene(SceneHost& host, const std::string& heading, const std::string& title,
             const std::vector<SceneLine>& lines, SceneResolver resolve = nullptr);
  void onInput(const InputEvent& e) override;
  void update(double dt) override;
  void draw(Renderer& r) override;
  SceneAudio audioContext() const override { SceneAudio a; a.kind = SceneAudio::Scene; return a; }  // scene music

 private:
  bool hasCard() const { return !heading_.empty() || !title_.empty(); }
  void openLine();
  void lineResult(DialogueBox::Result res);
  void leaveCard();
  void skipRest();  // skip everything that is left, but stop at the next question
  void answered(int pick);
  void finish();
  void consumeAudio(bool fx);
  void playAudio(const SceneLine& l);
  // Scene presentation (docs/EVENTS.md "Scene presentation"): an Fx line acts when reached; fades, waits, shakes and background
  // transitions hold the scene (blockLeft_) until they are over. skipFx = the skip control: instant.
  void applyFx(const SceneLine& l, bool instant);
  void applyRest(size_t from, size_t to);   // skip: the music and presentation of the lines in [from, to) take effect at once
  void endScene();
  const StageDrawCtx stageCtx() const;

  SceneHost& host_;
  std::string heading_, title_;
  std::vector<SceneLine> lines_;
  size_t next_ = 0;       // the line to open next
  bool entered_ = false;  // update() has run: the music director knows this scene
  std::vector<SceneLine> queued_;  // sound requests reached before that
  bool onCard_ = false;
  bool finished_ = false;
  float cardTime_ = 0;    // how long the card has been up
  float lineTime_ = 0;    // autoplay: how long the line has been up
  SceneResolver resolve_;
  std::vector<int> picks_;  // the answers given so far (a question's position among the answers offered)
  SceneStage stage_;
  float blockLeft_ = 0;       // seconds a fade / wait / shake / transition still holds the next line back
  bool ending_ = false;       // the scene is over but faded out: it fades in before it leaves
  float holdT_ = -1;          // Cancel / Menu held while a block runs (seconds), -1 = not held
  DialogueBox box_;
  Color bg_;
};

}  // namespace ote
