// The generic JSON tree tab, for every file that is not a table (game.json, maps, anything): expand, edit scalars in
// place, rename keys, add and remove members, change a value's type. All edits share the project's undo stack.
#pragma once
#include <QWidget>
#include <functional>

#include "json_document.h"
#include "schema.h"

class QLabel;
class QTabWidget;
class QTreeView;

namespace ote_native {

class FormContext;
class FormView;
class IdSource;
class JsonTreeModel;
class MapPage;
class AppSettings;
class Project;

class TreeEditor : public QWidget {
  Q_OBJECT
 public:
  // With a schema for the whole document (game.json) the editor has two pages: the schema form and the generic tree.
  TreeEditor(Project* project, JsonDocument* doc, QWidget* parent = nullptr, const FileSchema* schema = nullptr,
             IdSource* ids = nullptr);

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

  FormView* form() const { return form_; }
  FormContext* formContext() const { return formCtx_; }
  QTabWidget* pages() const { return pages_; }
  // Map files only: the painter page (the first page; null for every other file).
  MapPage* mapPage() const { return mapPage_; }
  void showMapPage();
  void setAppSettings(AppSettings* settings);  // lets the painter remember zoom / tool
  void showFormPage();
  void showTreePage();
  // Jumps to what a problem message names ("rules.actionPoints", "damageTypes[1]", a quoted key) in the form, else the tree.
  bool locateProblem(const QString& message);

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
  QTabWidget* pages_ = nullptr;
  QWidget* treePage_ = nullptr;
  FormContext* formCtx_ = nullptr;
  FormView* form_ = nullptr;
  MapPage* mapPage_ = nullptr;
  const FileSchema* schema_ = nullptr;
};

}  // namespace ote_native
