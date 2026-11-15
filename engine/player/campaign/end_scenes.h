// The campaign's two small screens that are not scenes: the end screen ("The End" / "The story ends
// here.") and the choice after a lost chapter ("Try again" / "Back to title").
#pragma once
#include <string>

#include "scene.h"
#include "ui/row_list.h"

namespace ote {

// A centred message on the style's background; Confirm / tap returns to the game page. With
// --autoplay it leaves by itself after a second.
class EndScene : public Scene {
 public:
  EndScene(SceneHost& host, const std::string& big, const std::string& small);
  void onInput(const InputEvent& e) override;
  void update(double dt) override;
  void draw(Renderer& r) override;
  SceneAudio audioContext() const override { SceneAudio a; a.kind = SceneAudio::Title; return a; }  // the game's title music

 private:
  void leave();
  SceneHost& host_;
  std::string big_, small_;
  Color bg_;
  float time_ = 0;
  bool left_ = false;
};

// After a lost chapter whose campaign says "retry". Try again starts the chapter's battle anew (the
// before scene is not replayed); Back to title returns to the game page, where Continue picks the
// chapter up. In --autoplay it tries again by itself.
class RetryScene : public Scene {
 public:
  RetryScene(SceneHost& host, const std::string& heading, const std::string& title);
  void onInput(const InputEvent& e) override;
  void update(double dt) override;
  void draw(Renderer& r) override;

 private:
  void layout(const Renderer& r);
  void choose(int row);
  SceneHost& host_;
  std::string heading_, title_;
  Color bg_;
  RowList list_;
  float time_ = 0;
  bool left_ = false;
};

// The background colour of the style in use (like the battle's), for the campaign's own screens.
Color storyBackground(SceneHost& host);

}  // namespace ote
