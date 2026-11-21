// A form generated from a schema object node, for ONE object of the document (a table row, or the whole of game.json).
// Layout: raw key as the label, the field, a one-line help under it; lists with add / remove / reorder, maps keyed by
// ids with one row per id, optional blocks with an Enable switch, and an "Other keys" box for what the schema does not know.
// The form never keeps values of its own: it follows the document's change signals (value changes refresh one field,
// changes of shape rebuild the form on the next event-loop turn, keeping scroll position and focus).
#pragma once
#include <QHash>
#include <QPointer>
#include <QScrollArea>

#include "form_fields.h"

class QLabel;

namespace ote_native {

class FormView : public QScrollArea {
  Q_OBJECT
 public:
  // `node` is the Object node describing the target. The target is chosen with setTarget().
  FormView(FormContext* ctx, const SchemaNode* node, QWidget* parent = nullptr);

  // Shows the object at `path` (empty path = the document root). A path that does not resolve clears the form.
  void setTarget(const JsonPath& path);
  void clearTarget();
  const JsonPath& target() const { return target_; }
  bool hasTarget() const { return hasTarget_; }

  // The registered leaf field for a path, nullptr when none.
  FieldBase* fieldAt(const JsonPath& path) const;
  // Gives the field of `path` (or its nearest shown ancestor) the keyboard focus and scrolls it into view.
  bool focusPath(const JsonPath& path);
  // Focus the field of this key of the target object ("apCost"), searching nested objects too.
  bool focusKey(const QString& key);
  // Runs a pending rebuild now (tests; the form otherwise does it on the next event-loop turn).
  void flush();
  QList<FieldBase*> fields() const;
  QWidget* body() const { return body_; }
  QLabel* messageLabel() const { return message_; }

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

 private:
  void rebuild();
  void onChanged(const JsonChange& c);
  QWidget* buildList(const SchemaNode& node, const JsonPath& path);
  QWidget* buildMap(const SchemaNode& node, const JsonPath& path);
  QWidget* buildBlock(const SchemaNode& node, const JsonPath& path);
  QWidget* buildOtherKeys(const SchemaNode& node, const JsonPath& path);
  QWidget* buildLeaf(const SchemaNode& node, const JsonPath& path);

  FormContext* ctx_;
  const SchemaNode* node_;
  JsonPath target_;
  bool hasTarget_ = false;
  bool rebuildPending_ = false;
  QWidget* body_ = nullptr;
  QLabel* message_ = nullptr;
  QHash<QString, QPointer<FieldBase>> fields_;
  QHash<QString, QPointer<QWidget>> anchors_;  // containers (lists, blocks, objects) by path, to scroll to
};

}  // namespace ote_native
