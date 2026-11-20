// The generic JSON tree tab, for every file that is not a table (game.json, maps, anything): expand, edit scalars in
// place, rename keys, add and remove members, change a value's type. All edits share the project's undo stack.
#pragma once
#include <QWidget>
#include <functional>

#include "json_document.h"

class QLabel;
class QTreeView;

namespace ote_native {

class JsonTreeModel;
class Project;

class TreeEditor : public QWidget {
  Q_OBJECT
 public:
  TreeEditor(Project* project, JsonDocument* doc, QWidget* parent = nullptr);

  QTreeView* view() const { return view_; }
  JsonTreeModel* model() const { return model_; }
  QLabel* messageLabel() const { return message_; }

  // Adds a value (of the given default type) inside the current container, or beside the current scalar. For objects the
  // key comes from the prompt hook (default: a small input dialog). Returns false when cancelled or refused.
  bool addChild(ote::Json::Type type);
  bool removeCurrent();
  // Replace the current node with a fresh value of another type (the old value is gone from the file, but Undo restores it).
  bool changeCurrentType(ote::Json::Type type);
  void selectPath(const JsonPath& path);  // used when a problem names a spot

  using KeyPrompt = std::function<QString(const QString& title)>;  // empty string = cancelled
  void setKeyPrompt(KeyPrompt prompt) { keyPrompt_ = std::move(prompt); }
  void showMessage(const QString& text, bool isError);

 signals:
  void messageShown(const QString& text);

 private:
  void showContextMenu(const QPoint& pos);
  QModelIndex containerForAdd() const;

  JsonDocument* doc_;
  JsonTreeModel* model_;
  QTreeView* view_;
  QLabel* message_;
  KeyPrompt keyPrompt_;
};

}  // namespace ote_native
