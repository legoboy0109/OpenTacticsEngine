// The widgets a schema form is built from. A field knows its schema node and the path of its value; it reads the value
// from the document (never keeps its own copy) and changes it only through FormContext. Leaf fields refresh themselves
// when their value changes; anything that changes the shape (list items, blocks) makes the FormView rebuild.
#pragma once
#include <QFrame>
#include <QWidget>

#include "form_context.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QToolButton;
class QVBoxLayout;

namespace ote_native {

class FormView;

// Colours that read on light and dark palettes: red like the table's refusals, muted text from the palette.
QString warningStyle();
QColor mutedColor(const QWidget* w);

class FieldBase : public QWidget {
  Q_OBJECT
 public:
  FieldBase(FormContext* ctx, FormView* view, const SchemaNode* node, JsonPath path, QWidget* parent);

  const JsonPath& path() const { return path_; }
  const SchemaNode* node() const { return node_; }
  // Re-read the value from the document (a leaf field only; containers are rebuilt instead).
  virtual void refresh() {}
  // The widget that takes the keyboard focus for this field.
  virtual QWidget* focusTarget() { return this; }
  const ote::Json* current() const { return ctx_->at(path_); }

 protected:
  FormContext* ctx_;
  FormView* view_;
  const SchemaNode* node_;
  JsonPath path_;
};

// Text, whole number or number in one line. Empty text means "key absent" (or "" for a required text key). Wrong input is
// refused with a red note under the field and nothing is written.
class ScalarField : public FieldBase {
  Q_OBJECT
 public:
  ScalarField(FormContext* ctx, FormView* view, const SchemaNode* node, JsonPath path, QWidget* parent);
  void refresh() override;
  QWidget* focusTarget() override;
  QLineEdit* edit() const { return edit_; }

 private:
  void commit();
  void fail(const QString& why);

  QLineEdit* edit_;
  QToolButton* clear_;
  QLabel* note_;
  QString shown_;
};

class BoolField : public FieldBase {
  Q_OBJECT
 public:
  BoolField(FormContext* ctx, FormView* view, const SchemaNode* node, JsonPath path, QWidget* parent);
  void refresh() override;
  QWidget* focusTarget() override;
  QCheckBox* box() const { return box_; }

 private:
  QCheckBox* box_;
  QToolButton* clear_;
};

// Enum words or the ids of another file, in a combo box. A stored value the list does not have is kept and marked.
class ChoiceField : public FieldBase {
  Q_OBJECT
 public:
  ChoiceField(FormContext* ctx, FormView* view, const SchemaNode* node, JsonPath path, QWidget* parent);
  void refresh() override;
  void refreshItems();  // the id list changed
  QWidget* focusTarget() override;
  QComboBox* combo() const { return combo_; }

 private:
  QStringList items() const;
  QComboBox* combo_;
  QToolButton* clear_;
  QLabel* note_;
};

// A fixed-length list of numbers or words: range [min, max], resistRange.
class TupleField : public FieldBase {
  Q_OBJECT
 public:
  TupleField(FormContext* ctx, FormView* view, const SchemaNode* node, JsonPath path, QWidget* parent);
  void refresh() override;
  QWidget* focusTarget() override;
  const QList<QLineEdit*>& edits() const { return edits_; }

 private:
  void commit();
  QList<QLineEdit*> edits_;
  QToolButton* clear_;
  QLabel* note_;
};

// Anything the schema leaves as raw JSON: shown compactly, edited in the JSON dialog.
class JsonField : public FieldBase {
  Q_OBJECT
 public:
  JsonField(FormContext* ctx, FormView* view, const SchemaNode* node, JsonPath path, QWidget* parent, const QString& warning = QString());
  void refresh() override;
  QWidget* focusTarget() override;

 private:
  void edit();
  QLineEdit* line_;
  QToolButton* clear_;
  QString warning_;
};

}  // namespace ote_native
