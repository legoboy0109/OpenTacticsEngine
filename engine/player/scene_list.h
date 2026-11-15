// Shared list-with-title scene (map select), plus the error screen.
// Concrete scenes override onChoose / onCancel.
#pragma once
#include <string>
#include <vector>

#include "scene.h"
#include "ui/row_list.h"

namespace ote {

struct ListEntry {
  std::string label;
  std::string detail;  // grey, drawn under the label
};

class ListScene : public Scene {
 public:
  ListScene(SceneHost& host, std::string title, std::vector<ListEntry> entries, std::string footer);
  void onInput(const InputEvent& e) override;
  void draw(Renderer& r) override;
  SceneAudio audioContext() const override { SceneAudio a; a.kind = SceneAudio::Title; return a; }  // the game's title music

 protected:
  virtual void onChoose(int index) = 0;
  virtual void onCancel() {}
  void select(int index) { list_.sel = index < 0 || index >= (int)entries_.size() ? 0 : index; }

  SceneHost& host_;
  std::string title_;
  std::vector<ListEntry> entries_;
  std::string footer_;

 private:
  void layout(const Renderer& r);  // fills list_ from the screen size and the active font
  RowList list_;
};

// Shows a (possibly long) message in wrapped text; Cancel/Confirm/tap goes back.
class ErrorScene : public Scene {
 public:
  ErrorScene(SceneHost& host, std::string title, std::string message, SceneHost::Back back)
      : host_(host), title_(std::move(title)), message_(std::move(message)), back_(back) {}
  void onInput(const InputEvent& e) override;
  void draw(Renderer& r) override;

 private:
  SceneHost& host_;
  std::string title_, message_;
  SceneHost::Back back_;
};

}  // namespace ote
