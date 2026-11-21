// A form generated from a schema object node, for ONE object of the document (a table row, or the whole of game.json).
// Layout: raw key as the label, the field, a one-line help under it; lists with add / remove / reorder, maps keyed by
// ids with one row per id (or free names), optional blocks with an Enable switch, one-of-several shapes (variants) with a
// picker, and an "Other keys" box for what the schema does not know. With enough width the fields flow in two columns.
// The form never keeps values of its own: it follows the document's change signals (value changes refresh one field,
// changes of shape rebuild the form on the next event-loop turn, keeping scroll position and focus).
//
// Speed: blocks and list items are built lazily. A collapsed one has a header only; its body is instantiated when it is
// first opened. Past a budget of built nodes everything new starts collapsed, so a form with dozens of optional blocks or a
// long list of commands opens instantly; what the user opened or closed is remembered (per path) across rebuilds.
#pragma once
#include <QHash>
#include <QList>
#include <QPair>
#include <QPointer>
#include <QScrollArea>
#include <QSet>
#include <functional>

#include "form_fields.h"

class QBoxLayout;
class QFrame;
class QLabel;
class QTimer;
class QToolButton;

namespace ote_native {

class FormView : public QScrollArea {
  Q_OBJECT
 public:
  // `node` is the Object node describing the target. The target is chosen with setTarget().
  FormView(FormContext* ctx, const SchemaNode* node, QWidget* parent = nullptr);
  ~FormView() override;

  // Shows the object at `path` (empty path = the document root). A path that does not resolve clears the form.
  void setTarget(const JsonPath& path);
  void clearTarget();
  const JsonPath& target() const { return target_; }
  bool hasTarget() const { return hasTarget_; }

  // The registered leaf field for a path, nullptr when none.
  FieldBase* fieldAt(const JsonPath& path) const;
  // Gives the field of `path` (or its nearest shown ancestor) the keyboard focus and scrolls it into view; opens the
  // collapsed blocks on the way.
  bool focusPath(const JsonPath& path);
  // Focus the field of this key of the target object ("apCost"), searching nested objects too.
  bool focusKey(const QString& key);
  // True when a container (object, list, command ...) of this path is built in the form right now.
  bool hasAnchor(const JsonPath& path) const { return anchors_.value(pathToString(path)) != nullptr; }
  // The widget of such a container (screenshots and tests scroll to it), or nullptr.
  QWidget* anchorWidget(const JsonPath& path) const { return anchors_.value(pathToString(path)).data(); }
  // Runs a pending rebuild now (tests; the form otherwise does it on the next event-loop turn).
  void flush();
  QList<FieldBase*> fields() const;
  QWidget* body() const { return body_; }
  QLabel* messageLabel() const { return message_; }

  // Opens every collapsed block / list item (builds everything). Tests and "find a key" use it.
  void expandAll();
  // Opens the collapsed containers on the way to `path`; true when something was opened.
  bool expandAlong(const JsonPath& path);
  // Number of schema nodes instantiated by the last build, and how many containers are still collapsed.
  int builtNodes() const { return built_; }
  int collapsedCount() const { return (int)expanders_.size(); }
  // 1 or 2: how many columns of fields the form uses (two from kWideForm pixels on).
  int columns() const { return columns_; }
  static constexpr int kWideForm = 900;
  static constexpr int kBudget = 220;  // nodes built before new containers start collapsed
  static constexpr size_t kPageSize = 50;  // list items built at once; longer lists get a page bar

  // --- moving list items (form_drag.cpp) ---
  // Moves item `from` of the list at `srcList` to `to` (the index it is inserted before, counted before the move) of the same list
  // or of another list of this file with items of the same kind: one undo step. The items that were open stay open, and the
  // keyboard focus goes with the item: to its field, or to the button named `focusButton` (the grip, "itemUp", "itemDown").
  bool moveListItem(const JsonPath& srcList, size_t from, const JsonPath& dstList, size_t to, const QString& focusButton = QStringLiteral("dragHandle"));
  // The grip of an item (tests press and drag it), nullptr when that item is not built.
  QWidget* dragHandleAt(const JsonPath& list, size_t index) const;
  bool isDragging() const { return drag_ != nullptr; }
  // While dragging: where the item would go (the list's path and index), or false when it would not move.
  bool dropTarget(JsonPath* list, size_t* index) const;

  // --- used by the fields ---
  void registerField(FieldBase* f);
  void requestRebuild();
  void showInTree(const JsonPath& path) { emit showInTreeRequested(path); }
  FormContext* context() const { return ctx_; }
  // Builds the widget for any node (dispatch by kind). `inList`: the value is a list item (no clear button).
  QWidget* buildNode(const SchemaNode& node, const JsonPath& path, bool inList = false);
  // The grid of an object's fields plus its "Other keys" box.
  QWidget* buildObjectBody(const SchemaNode& node, const JsonPath& path);

 signals:
  void showInTreeRequested(const JsonPath& path);

 protected:
  void resizeEvent(QResizeEvent* e) override;

 private:
  void rebuild();
  void onChanged(const JsonChange& c);
  QWidget* buildList(const SchemaNode& node, const JsonPath& path);
  QToolButton* makeDragHandle(const JsonPath& list, size_t index, QWidget* parent);
  void registerListItem(const JsonPath& list, size_t index, QFrame* row);
  void beginDrag(const JsonPath& list, size_t index, QPoint global);
  void updateDrag(QPoint global);
  void endDrag(bool drop);
  QString currentFocusKey() const;
  QWidget* buildMap(const SchemaNode& node, const JsonPath& path);
  QWidget* buildFreeMap(const SchemaNode& node, const JsonPath& path);
  QWidget* buildBlock(const SchemaNode& node, const JsonPath& path);
  QWidget* buildVariant(const SchemaNode& node, const JsonPath& path);
  QWidget* buildOtherKeys(const SchemaNode& node, const JsonPath& path);
  QWidget* buildLeaf(const SchemaNode& node, const JsonPath& path);
  // A container body that is built when first opened. The toggle button is added to `header`; returns the holder widget
  // (to put under the header). `make` builds the body.
  QWidget* lazyBody(const JsonPath& path, QBoxLayout* header, const std::function<QWidget*()>& make, bool startClosed = false);

  FormContext* ctx_;
  const SchemaNode* node_;
  JsonPath target_;
  bool hasTarget_ = false;
  bool rebuildPending_ = false;
  QWidget* body_ = nullptr;
  QLabel* message_ = nullptr;
  int columns_ = 1;
  int bodyDepth_ = 0;
  int built_ = 0;
  QHash<QString, QPointer<FieldBase>> fields_;
  QHash<QString, QPointer<QWidget>> anchors_;  // containers (lists, blocks, objects) by path, to scroll to
  QHash<QString, std::function<void()>> expanders_;  // collapsed containers: opens one
  QSet<QString> open_, closed_;                      // what the user opened / closed (survives rebuilds)
  QHash<QString, size_t> pages_;                     // the page each long list shows (survives rebuilds)
  // List items, for moving them: the built lists with their rows, the rows by item path, the drag in progress, and where the
  // focus goes after the next rebuild (a move changes every path below it).
  struct ListRows {
    QPointer<QWidget> widget;
    const SchemaNode* item = nullptr;
    JsonPath path;
    QList<QPair<size_t, QPointer<QFrame>>> rows;
  };
  struct DragState;
  QHash<QString, ListRows> lists_;
  QHash<QString, QPointer<QFrame>> itemRows_;
  QHash<QString, bool> lazyOpen_;  // every collapsible container built, and whether it is open now
  DragState* drag_ = nullptr;
  QString pendingFocusKey_, pendingFocusItem_, pendingFocusButton_;
  QSet<QString> discriminators_;  // paths whose change alters which fields / shape apply: the form is rebuilt for them
};

}  // namespace ote_native
