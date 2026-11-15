// A campaign scene on its own screen (docs/EVENTS.md "Campaign"): the style's background colour, an
// optional title card ("Chapter N" over the chapter's name) and then the scene's lines in the same
// DialogueBox a battle uses, with the same controls: Confirm / tap = finish the page, turn it, end the
// line; holding Cancel or Menu, the Skip tab or the `skip` script token = skip the rest of the scene.
// In --autoplay the card and every line pass by themselves so smoke tests never hang on a scene.
#pragma once
#include <string>
#include <vector>

#include "core/campaign/playthrough.h"
#include "scene.h"
#include "ui/dialogue_box.h"

namespace ote {

class StoryScene : public Scene {
 public:
  // `heading` / `title` = the card ("Chapter 2" / the chapter's name); both empty = no card.
  // At least one of card and lines is expected (an empty scene is not shown at all).
  StoryScene(SceneHost& host, const std::string& heading, const std::string& title,
             const std::vector<SceneLine>& lines);
  void onInput(const InputEvent& e) override;
  void update(double dt) override;
  void draw(Renderer& r) override;
  SceneAudio audioContext() const override { SceneAudio a; a.kind = SceneAudio::Scene; return a; }  // scene music

 private:
  bool hasCard() const { return !heading_.empty() || !title_.empty(); }
  void openLine();
  void lineResult(DialogueBox::Result res);
  void leaveCard();
  void finish();
  void consumeAudio();
  void playAudio(const SceneLine& l);

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
  DialogueBox box_;
  Color bg_;
};

}  // namespace ote
