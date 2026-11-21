#include "form_fields.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPalette>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>

#include "cell_text.h"
#include "form_view.h"

namespace ote_native {

QString warningStyle() { return QStringLiteral("color: #e5534b;"); }

QColor mutedColor(const QWidget* w) {
  // Placeholder text is the palette's own "quiet" colour, so it stays readable in light and dark themes.
  return w->palette().color(QPalette::PlaceholderText);
}

namespace {

QToolButton* makeClearButton(QWidget* parent, const SchemaNode* node) {
  auto* b = new QToolButton(parent);
  b->setObjectName("clearField");
  b->setText(QStringLiteral("×"));
  b->setToolTip(node->required ? QObject::tr("This key is required") : QObject::tr("Remove this key from the file (the game then uses its default)"));
  b->setAutoRaise(true);
  return b;
}

QLabel* makeNote(QWidget* parent) {
  auto* l = new QLabel(parent);
  l->setObjectName("fieldNote");
  l->setWordWrap(true);
  l->setStyleSheet(warningStyle());
  l->hide();
  return l;
}

void showNote(QLabel* l, const QString& text) {
  l->setText(text);
  l->setVisible(!text.isEmpty());
}

QString placeholderFor(const SchemaNode* n) {
  if (!n->deflt.isNull()) return QObject::tr("default: %1").arg(jsonToCellText(n->deflt));
  return n->required ? QObject::tr("required") : QObject::tr("not set");
}

QString rangeText(const SchemaNode* n) {
  if (n->min && n->max) return QObject::tr("from %1 to %2").arg(*n->min).arg(*n->max);
  if (n->min) return QObject::tr("at least %1").arg(*n->min);
  if (n->max) return QObject::tr("at most %1").arg(*n->max);
  return {};
}

// Editor + clear button side by side, then an optional note line under them.
QVBoxLayout* rowLayout(QWidget* host, QWidget* editor, QToolButton* clear, QLabel* note) {
  auto* v = new QVBoxLayout(host);
  v->setContentsMargins(0, 0, 0, 0);
  v->setSpacing(1);
  auto* h = new QHBoxLayout;
  h->setContentsMargins(0, 0, 0, 0);
  h->addWidget(editor, 1);
  if (clear) h->addWidget(clear);
  v->addLayout(h);
  if (note) v->addWidget(note);
  return v;
}

}  // namespace

FieldBase::FieldBase(FormContext* ctx, FormView* view, const SchemaNode* node, JsonPath path, QWidget* parent)
    : QWidget(parent), ctx_(ctx), view_(view), node_(node), path_(std::move(path)) {
  if (view_) view_->registerField(this);
}

// --- ScalarField ------------------------------------------------------------------------------------------------

ScalarField::ScalarField(FormContext* ctx, FormView* view, const SchemaNode* node, JsonPath path, QWidget* parent)
    : FieldBase(ctx, view, node, std::move(path), parent) {
  setObjectName("field_" + pathToString(path_));
  edit_ = new QLineEdit(this);
  edit_->setObjectName("scalarEdit");
  edit_->setPlaceholderText(placeholderFor(node_));
  if (node_->kind != SchemaNode::Kind::String) edit_->setAlignment(Qt::AlignRight);
  clear_ = makeClearButton(this, node_);
  clear_->setEnabled(false);
  note_ = makeNote(this);
  rowLayout(this, edit_, clear_, note_);
  connect(edit_, &QLineEdit::editingFinished, this, &ScalarField::commit);
  connect(clear_, &QToolButton::clicked, this, [this] { ctx_->erase(path_); });
  refresh();
}

QWidget* ScalarField::focusTarget() { return edit_; }

void ScalarField::refresh() {
  const ote::Json* v = current();
  if (v && !typeOk(*node_, *v)) {  // became the wrong kind (undo, another view): the form is rebuilt with the raw-JSON field
    if (view_) view_->requestRebuild();
    return;
  }
  shown_ = v ? jsonToCellText(*v) : QString();
  edit_->setText(shown_);
  clear_->setEnabled(v && !node_->required);
  QString why;
  const bool bad = v && !nodeAccepts(*node_, *v, &why);
  // A value outside the range is kept and marked; nothing here ever changes it on its own.
  showNote(note_, bad ? tr("Outside the range (%1): %2. The file keeps it as it is.").arg(rangeText(node_), why) : QString());
}

void ScalarField::fail(const QString& why) {
  showNote(note_, why);
  edit_->setStyleSheet("border: 1px solid #e5534b;");
}

void ScalarField::commit() {
  const QString text = edit_->text().trimmed();
  edit_->setStyleSheet(QString());
  if (text == shown_) return;
  if (text.isEmpty()) {
    if (node_->kind == SchemaNode::Kind::String && node_->required) ctx_->set(path_, ote::Json::makeString(""));
    else if (node_->required) return fail(tr("This key is required."));
    else ctx_->erase(path_);
    return;
  }
  if (node_->kind == SchemaNode::Kind::String) {
    ctx_->set(path_, ote::Json::makeString(edit_->text().toStdString()));
    return;
  }
  bool ok = false;
  const double d = QLocale::c().toDouble(text, &ok);
  if (!ok || std::isnan(d) || std::isinf(d)) return fail(tr("\"%1\" is not a number.").arg(text));
  if (node_->kind == SchemaNode::Kind::Int && d != std::floor(d)) return fail(tr("This key takes a whole number."));
  if ((node_->min && d < *node_->min) || (node_->max && d > *node_->max))
    return fail(tr("Must be %1.").arg(rangeText(node_)));
  ctx_->set(path_, ote::Json::makeNumber(d));
}

// --- BoolField --------------------------------------------------------------------------------------------------

BoolField::BoolField(FormContext* ctx, FormView* view, const SchemaNode* node, JsonPath path, QWidget* parent)
    : FieldBase(ctx, view, node, std::move(path), parent) {
  setObjectName("field_" + pathToString(path_));
  box_ = new QCheckBox(this);
  box_->setObjectName("boolBox");
  clear_ = makeClearButton(this, node_);
  rowLayout(this, box_, clear_, nullptr);
  connect(box_, &QCheckBox::clicked, this, [this](bool on) { ctx_->set(path_, ote::Json::makeBool(on)); });
  connect(clear_, &QToolButton::clicked, this, [this] { ctx_->erase(path_); });
  refresh();
}

QWidget* BoolField::focusTarget() { return box_; }

void BoolField::refresh() {
  const ote::Json* v = current();
  if (v && !typeOk(*node_, *v)) {
    if (view_) view_->requestRebuild();
    return;
  }
  const bool on = v ? v->asBool() : node_->deflt.asBool();
  box_->setChecked(on);
  // An absent key shows the engine's default, worded so it is clear nothing is written yet.
  box_->setText(v ? QString() : tr("(not set: %1)").arg(on ? tr("on") : tr("off")));
  clear_->setEnabled(v && !node_->required);
}

// --- ChoiceField ------------------------------------------------------------------------------------------------

ChoiceField::ChoiceField(FormContext* ctx, FormView* view, const SchemaNode* node, JsonPath path, QWidget* parent)
    : FieldBase(ctx, view, node, std::move(path), parent) {
  setObjectName("field_" + pathToString(path_));
  combo_ = new QComboBox(this);
  combo_->setObjectName("choiceCombo");
  combo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  combo_->setMinimumContentsLength(10);
  clear_ = makeClearButton(this, node_);
  note_ = makeNote(this);
  rowLayout(this, combo_, clear_, note_);
  connect(combo_, &QComboBox::activated, this, [this](int i) {
    const QString id = combo_->itemData(i).toString();
    if (id.isEmpty()) ctx_->erase(path_);
    else ctx_->set(path_, ote::Json::makeString(id.toStdString()));
  });
  connect(clear_, &QToolButton::clicked, this, [this] { ctx_->erase(path_); });
  refreshItems();
}

QWidget* ChoiceField::focusTarget() { return combo_; }

QStringList ChoiceField::items() const {
  if (node_->kind == SchemaNode::Kind::Enum) {
    QStringList l;
    for (const std::string& s : node_->values) l << QString::fromStdString(s);
    return l;
  }
  QStringList l = ctx_->ids() ? ctx_->ids()->ids(node_->refs) : QStringList();
  for (const std::string& s : node_->values) l << QString::fromStdString(s);  // words such as "choose" beside the ids
  return l;
}

void ChoiceField::refreshItems() { refresh(); }

void ChoiceField::refresh() {
  const ote::Json* v = current();
  if (v && !typeOk(*node_, *v)) {
    if (view_) view_->requestRebuild();
    return;
  }
  const QString cur = v ? QString::fromStdString(v->asString()) : QString();
  const QStringList list = items();
  combo_->blockSignals(true);
  combo_->clear();
  combo_->addItem(node_->deflt.isString() ? tr("(default: %1)").arg(QString::fromStdString(node_->deflt.asString()))
                                          : (node_->required ? tr("(choose)") : tr("(not set)")),
                  QString());
  for (const QString& s : list) combo_->addItem(s, s);
  const bool known = list.contains(cur);
  if (v && !known) {
    combo_->addItem(tr("%1 (unknown)").arg(cur), cur);
    combo_->setItemData(combo_->count() - 1, QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning), Qt::DecorationRole);
  }
  combo_->setCurrentIndex(!v ? 0 : combo_->findData(cur));
  combo_->blockSignals(false);
  clear_->setEnabled(v && !node_->required);
  showNote(note_, (v && !known) ? (node_->kind == SchemaNode::Kind::Enum
                                       ? tr("\"%1\" is not one of the allowed words. The file keeps it as it is.").arg(cur)
                                       : tr("No \"%1\" exists in %2 (yet). The file keeps it as it is.").arg(cur, QString::fromStdString(node_->refs.front())))
                                : QString());
}

// --- TupleField -------------------------------------------------------------------------------------------------

TupleField::TupleField(FormContext* ctx, FormView* view, const SchemaNode* node, JsonPath path, QWidget* parent)
    : FieldBase(ctx, view, node, std::move(path), parent) {
  setObjectName("field_" + pathToString(path_));
  auto* host = new QWidget(this);
  auto* h = new QHBoxLayout(host);
  h->setContentsMargins(0, 0, 0, 0);
  for (int i = 0; i < node_->length; ++i) {
    auto* e = new QLineEdit(host);
    e->setObjectName(QStringLiteral("tupleEdit%1").arg(i));
    e->setAlignment(Qt::AlignRight);
    h->addWidget(e);
    edits_ << e;
    connect(e, &QLineEdit::editingFinished, this, &TupleField::commit);
  }
  clear_ = makeClearButton(this, node_);
  note_ = makeNote(this);
  rowLayout(this, host, clear_, note_);
  connect(clear_, &QToolButton::clicked, this, [this] { ctx_->erase(path_); });
  refresh();
}

QWidget* TupleField::focusTarget() { return edits_.value(0); }

void TupleField::refresh() {
  const ote::Json* v = current();
  if (v && !typeOk(*node_, *v)) {
    if (view_) view_->requestRebuild();
    return;
  }
  QString bad;
  for (int i = 0; i < edits_.size(); ++i) {
    edits_[i]->setText(v ? jsonToCellText((*v)[(size_t)i]) : QString());
    edits_[i]->setPlaceholderText(node_->of->deflt.isNull() ? tr("not set") : jsonToCellText(node_->of->deflt));
    QString why;
    if (v && !nodeAccepts(*node_->of, (*v)[(size_t)i], &why) && bad.isEmpty()) bad = tr("Item %1: %2. The file keeps it as it is.").arg(i + 1).arg(why);
  }
  clear_->setEnabled(v && !node_->required);
  showNote(note_, bad);
}

void TupleField::commit() {
  ote::Json arr = ote::Json::makeArray();
  const ote::Json* cur = current();
  for (int i = 0; i < edits_.size(); ++i) {
    const QString t = edits_[i]->text().trimmed();
    if (t.isEmpty()) {
      if (!cur) {  // a pair is written whole: wait until every item has a value
        showNote(note_, tr("Fill in all %1 values.").arg(edits_.size()));
        return;
      }
      showNote(note_, tr("Every item needs a value (use the × button to remove the whole key)."));
      return;
    }
    bool ok = false;
    const double d = QLocale::c().toDouble(t, &ok);
    if (node_->of->kind == SchemaNode::Kind::String) {
      arr.push(ote::Json::makeString(t.toStdString()));
    } else if (!ok || (node_->of->kind == SchemaNode::Kind::Int && d != std::floor(d))) {
      showNote(note_, tr("\"%1\" is not a valid number here.").arg(t));
      return;
    } else {
      arr.push(ote::Json::makeNumber(d));
    }
  }
  showNote(note_, QString());
  ctx_->set(path_, arr);
}

// --- JsonField --------------------------------------------------------------------------------------------------

JsonField::JsonField(FormContext* ctx, FormView* view, const SchemaNode* node, JsonPath path, QWidget* parent, const QString& warning)
    : FieldBase(ctx, view, node, std::move(path), parent), warning_(warning) {
  setObjectName("field_" + pathToString(path_));
  line_ = new QLineEdit(this);
  line_->setObjectName("jsonLine");
  line_->setReadOnly(true);
  line_->setPlaceholderText(tr("not set"));
  auto* editB = new QToolButton(this);
  editB->setObjectName("editJsonButton");
  editB->setText(tr("Edit JSON..."));
  clear_ = makeClearButton(this, node_);
  auto* v = new QVBoxLayout(this);
  v->setContentsMargins(0, 0, 0, 0);
  v->setSpacing(1);
  auto* h = new QHBoxLayout;
  h->setContentsMargins(0, 0, 0, 0);
  h->addWidget(line_, 1);
  h->addWidget(editB);
  h->addWidget(clear_);
  v->addLayout(h);
  if (!warning_.isEmpty()) {
    auto* w = makeNote(this);
    showNote(w, warning_);
    v->addWidget(w);
  }
  connect(editB, &QToolButton::clicked, this, &JsonField::edit);
  connect(clear_, &QToolButton::clicked, this, [this] { ctx_->erase(path_); });
  refresh();
}

QWidget* JsonField::focusTarget() { return line_; }

void JsonField::refresh() {
  const ote::Json* v = current();
  if (v && warning_.isEmpty() && !typeOk(*node_, *v)) {
    if (view_) view_->requestRebuild();
    return;
  }
  if (v && !warning_.isEmpty() && typeOk(*node_, *v)) {
    if (view_) view_->requestRebuild();
    return;
  }
  const QString text = v ? QString::fromStdString(v->dump(0)) : QString();
  line_->setText(text);
  line_->setToolTip(v ? QString::fromStdString(v->dump(2)) : QString());
  clear_->setEnabled(v && !node_->required);
}

void JsonField::edit() {
  const ote::Json* v = current();
  ote::Json result;
  const ote::Json initial = v ? *v : defaultValue(*node_, ctx_->lookup());
  if (!ctx_->editJson(tr("Edit %1").arg(pathToString(path_)), v ? *v : (initial.isNull() ? ote::Json::makeObject() : initial), &result, this)) return;
  ctx_->set(path_, result);
}

}  // namespace ote_native
