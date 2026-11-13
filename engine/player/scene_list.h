// Shared list-with-title scene used by game select and map select, plus the
// error screen. Concrete scenes override onChoose / onCancel.
#pragma once
#include <string>
#include <vector>

#include "scene.h"

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

 protected:
  virtual void onChoose(int index) = 0;
  virtual void onCancel() {}
  void select(int index) { sel_ = index; }

  SceneHost& host_;
  std::string title_;
  std::vector<ListEntry> entries_;
  std::string footer_;
  int sel_ = 0;

 private:
  int visibleRows(int screenH) const;
  int firstRow(int screenH) const;
  int rowAt(int x, int y) const;
  int lastW_ = 480, lastH_ = 270;  // from the last draw, for pointer hit tests
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
