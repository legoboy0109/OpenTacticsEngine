#include "form_view.h"

#include <QApplication>
#include <QCheckBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>
#include <algorithm>
#include <QUndoStack>
#include <QVBoxLayout>

#include "project.h"

namespace ote_native {

namespace {

QLabel* helpLabel(const SchemaNode& n, QWidget* parent) {
  auto* l = new QLabel(QString::fromStdString(n.help), parent);
  l->setObjectName("helpLabel");
  l->setWordWrap(true);
  l->setForegroundRole(QPalette::PlaceholderText);  // follows the palette, so it stays readable in dark mode too
  QFont f = l->font();
  f.setPointSizeF(f.pointSizeF() * 0.92);
  l->setFont(f);
  l->setVisible(!n.help.empty());
  return l;
}

QLabel* keyLabel(const QString& key, bool required, QWidget* parent) {
  auto* l = new QLabel(required ? key + QStringLiteral(" *") : key, parent);
  l->setObjectName("keyLabel");
  QFont f = l->font();
  f.setBold(true);
  l->setFont(f);
  l->setToolTip(required ? QObject::tr("Required key") : QString());
  l->setTextInteractionFlags(Qt::TextSelectableByMouse);
  return l;
}

QToolButton* smallButton(const QString& text, const QString& tip, QWidget* parent, const char* name) {
  auto* b = new QToolButton(parent);
  b->setObjectName(name);
  b->setText(text);
  b->setToolTip(tip);
  b->setAutoRaise(true);
  return b;
}

QString summaryOf(const ote::Json& item) {
  if (!item.isObject()) return {};
  for (const char* k : {"id", "name", "at", "job", "skill", "ability"}) {
    const ote::Json& v = item[k];
    if (v.isString() && !v.asString().empty()) return QString::fromStdString(v.asString());
    if (v.isNumber()) return QStringLiteral("%1 %2").arg(k).arg(v.asNumber());
  }
  return {};
}

JsonPath child(const JsonPath& p, const std::string& key) {
  JsonPath c = p;
  c.push_back(PathStep::member(key));
  return c;
}
JsonPath child(const JsonPath& p, size_t index) {
  JsonPath c = p;
  c.push_back(PathStep::at(index));
  return c;
}

bool isCompound(const SchemaNode& n) {
  switch (n.kind) {
    case SchemaNode::Kind::List: return n.length < 0;
    case SchemaNode::Kind::Object: case SchemaNode::Kind::Map: return true;
    default: return false;
  }
}

}  // namespace

FormView::FormView(FormContext* ctx, const SchemaNode* node, QWidget* parent) : QScrollArea(parent), ctx_(ctx), node_(node) {
  setObjectName("formView");
  setWidgetResizable(true);
  setFrameShape(QFrame::NoFrame);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  connect(ctx_->doc(), &JsonDocument::changed, this, &FormView::onChanged);
  connect(ctx_->doc(), &JsonDocument::reloaded, this, [this] { requestRebuild(); });
  connect(ctx_, &FormContext::refused, this, [this](const QString& m) {
    if (message_) { message_->setText(m); message_->show(); }
  });
  if (ctx_->ids())
    connect(ctx_->ids(), &IdSource::idsChanged, this, [this] {
      for (const auto& f : fields_)
        if (auto* c = qobject_cast<ChoiceField*>(f.data())) c->refreshItems();
    });
  rebuild();
}

void FormView::registerField(FieldBase* f) { fields_.insert(pathToString(f->path()), f); }

FieldBase* FormView::fieldAt(const JsonPath& path) const { return fields_.value(pathToString(path)).data(); }

QList<FieldBase*> FormView::fields() const {
  QList<FieldBase*> out;
  for (const auto& f : fields_)
    if (f) out << f.data();
  return out;
}

void FormView::setTarget(const JsonPath& path) {
  hasTarget_ = ctx_->at(path) != nullptr;
  target_ = path;
  rebuild();
}

void FormView::clearTarget() {
  hasTarget_ = false;
  target_.clear();
  rebuild();
}

void FormView::requestRebuild() {
  if (rebuildPending_) return;
  rebuildPending_ = true;
  QTimer::singleShot(0, this, [this] {
    if (rebuildPending_) rebuild();
  });
}

void FormView::flush() {
  if (rebuildPending_) rebuild();
}

void FormView::rebuild() {
  rebuildPending_ = false;
  if (hasTarget_ && !ctx_->at(target_)) hasTarget_ = false;
  const int scroll = verticalScrollBar()->value();
  QString focusKeyStr;
  if (QWidget* fw = QApplication::focusWidget())
    for (auto it = fields_.cbegin(); it != fields_.cend(); ++it)
      if (it.value() && it.value()->isAncestorOf(fw)) { focusKeyStr = it.key(); break; }

  QWidget* old = takeWidget();
  fields_.clear();
  anchors_.clear();
  auto* page = new QWidget;
  page->setObjectName("formBody");
  auto* v = new QVBoxLayout(page);
  v->setContentsMargins(6, 6, 6, 6);
  message_ = new QLabel(page);
  message_->setObjectName("formMessage");
  message_->setWordWrap(true);
  message_->setStyleSheet(warningStyle());
  message_->hide();
  v->addWidget(message_);
  if (hasTarget_) {
    v->addWidget(buildObjectBody(*node_, target_));
  } else {
    auto* l = new QLabel(tr("Nothing selected."), page);
    l->setForegroundRole(QPalette::PlaceholderText);
    v->addWidget(l);
  }
  v->addStretch(1);
  body_ = page;
  setWidget(page);
  if (old) old->deleteLater();
  if (scroll > 0) QTimer::singleShot(0, this, [this, scroll] { verticalScrollBar()->setValue(scroll); });
  if (!focusKeyStr.isEmpty())
    if (FieldBase* f = fields_.value(focusKeyStr).data()) f->focusTarget()->setFocus();
}

void FormView::onChanged(const JsonChange& c) {
  if (!hasTarget_) return;
  if (!pathStartsWith(c.path, target_) && !pathStartsWith(target_, c.path)) return;
  if (c.path.size() >= target_.size()) {
    // Inside the shown object: a change to one leaf's value refreshes just that field (keeps focus and typing position).
    JsonPath leaf = c.path;
    if (c.kind != JsonChange::Kind::Set && !c.key.empty()) leaf.push_back(PathStep::member(c.key));
    if (c.kind == JsonChange::Kind::Set || !c.key.empty())
      if (FieldBase* f = fieldAt(leaf)) {
        f->refresh();
        return;
      }
  }
  requestRebuild();
}

bool FormView::focusPath(const JsonPath& path) {
  flush();
  JsonPath p = path;
  while (true) {
    if (FieldBase* f = fieldAt(p)) {
      ensureWidgetVisible(f);
      f->focusTarget()->setFocus();
      return true;
    }
    if (QWidget* a = anchors_.value(pathToString(p)).data()) {
      ensureWidgetVisible(a);
      return true;
    }
    if (p.size() <= target_.size()) return false;
    p.pop_back();
  }
}

bool FormView::focusKey(const QString& key) {
  flush();
  const std::string k = key.toStdString();
  FieldBase* best = nullptr;
  size_t bestDepth = ~size_t(0);
  for (const auto& f : fields_) {
    if (!f || f->path().empty() || f->path().back().isIndex || f->path().back().key != k) continue;
    if (f->path().size() < bestDepth) { best = f; bestDepth = f->path().size(); }
  }
  if (best) return focusPath(best->path());
  for (auto it = anchors_.cbegin(); it != anchors_.cend(); ++it) {
    if (!it.value()) continue;
    const QString suffix = QStringLiteral(".") + key;
    if (it.key().endsWith(suffix) || it.key() == key) { ensureWidgetVisible(it.value()); return true; }
  }
  return false;
}

// --- building ---------------------------------------------------------------------------------------------------

QWidget* FormView::buildLeaf(const SchemaNode& n, const JsonPath& path) {
  using K = SchemaNode::Kind;
  const ote::Json* v = ctx_->at(path);
  if (v && !typeOk(n, *v)) {
    QString expected = SchemaNode::kindName(n.kind);
    if (n.kind == K::Int) expected = tr("a whole number");
    else if (n.kind == K::String || n.kind == K::Ref) expected = tr("text");
    else if (n.kind == K::Bool) expected = tr("true or false");
    else if (n.kind == K::Enum) expected = tr("a word");
    else if (n.kind == K::Number) expected = tr("a number");
    else if (n.kind == K::List) expected = n.length > 0 ? tr("a list of %1 items").arg(n.length) : tr("a list");
    else expected = tr("an object { }");
    return new JsonField(ctx_, this, &n, path, nullptr,
                         tr("Expected %1, but the file has %2. Shown as it is; edit it as JSON or remove it.").arg(expected, QString::fromStdString(v->dump(0)).left(60)));
  }
  switch (n.kind) {
    case K::String: case K::Int: case K::Number: return new ScalarField(ctx_, this, &n, path, nullptr);
    case K::Bool: return new BoolField(ctx_, this, &n, path, nullptr);
    case K::Enum: case K::Ref: return new ChoiceField(ctx_, this, &n, path, nullptr);
    case K::List: return new TupleField(ctx_, this, &n, path, nullptr);  // fixed length only
    default: return new JsonField(ctx_, this, &n, path, nullptr);
  }
}

QWidget* FormView::buildNode(const SchemaNode& n, const JsonPath& path, bool inList) {
  using K = SchemaNode::Kind;
  const ote::Json* v = ctx_->at(path);
  QWidget* w;
  if (v && isAlt(n, *v)) w = new JsonField(ctx_, this, &n, path, nullptr);  // the loader's other accepted form, as JSON
  else if (v && !typeOk(n, *v)) w = buildLeaf(n, path);
  else if (n.kind == K::List && n.length < 0) w = buildList(n, path);
  else if (n.kind == K::Map) w = buildMap(n, path);
  else if (n.kind == K::Object) w = n.block ? buildBlock(n, path) : buildObjectBody(n, path);
  else w = buildLeaf(n, path);
  if (inList && n.isScalar())
    if (auto* b = w->findChild<QToolButton*>("clearField")) b->hide();  // the list's own remove button does that job
  if (isCompound(n)) anchors_.insert(pathToString(path), w);
  return w;
}

QWidget* FormView::buildObjectBody(const SchemaNode& n, const JsonPath& path) {
  auto* w = new QWidget;
  w->setObjectName("objectBody");
  auto* g = new QGridLayout(w);
  g->setContentsMargins(0, 0, 0, 0);
  g->setHorizontalSpacing(10);
  g->setVerticalSpacing(7);
  g->setColumnMinimumWidth(0, 120);
  g->setColumnStretch(1, 1);
  int row = 0;
  for (const auto& f : n.fields) {
    const SchemaNode& fn = *f.second;
    const JsonPath fp = child(path, f.first);
    const QString key = QString::fromStdString(f.first);
    if (fn.kind == SchemaNode::Kind::Object && fn.block) {  // an optional block draws its own header (the Enable checkbox)
      g->addWidget(buildNode(fn, fp), row++, 0, 1, 2);
      continue;
    }
    if (isCompound(fn)) {
      // Lists, maps and sub-objects get their own full-width row under the key and its help.
      auto* head = new QWidget(w);
      auto* hv = new QVBoxLayout(head);
      hv->setContentsMargins(0, 4, 0, 0);
      hv->setSpacing(0);
      {
        auto* top = new QHBoxLayout;
        top->setContentsMargins(0, 0, 0, 0);
        top->addWidget(keyLabel(key, fn.required, head));
        top->addStretch(1);
        const ote::Json* cur = ctx_->at(fp);
        if (cur && !fn.required && fn.kind != SchemaNode::Kind::Object) {
          auto* x = smallButton(QStringLiteral("×"), tr("Remove this key from the file"), head, "clearField");
          connect(x, &QToolButton::clicked, this, [this, fp] { ctx_->erase(fp); });
          top->addWidget(x);
        }
        hv->addLayout(top);
        hv->addWidget(helpLabel(fn, head));
      }
      g->addWidget(head, row++, 0, 1, 2);
      QWidget* content = buildNode(fn, fp);
      auto* indent = new QWidget(w);
      auto* il = new QHBoxLayout(indent);
      il->setContentsMargins(14, 0, 0, 0);
      il->addWidget(content);
      g->addWidget(indent, row++, 0, 1, 2);
    } else {
      g->addWidget(keyLabel(key, fn.required, w), row, 0, Qt::AlignTop);
      auto* cell = new QWidget(w);
      auto* cv = new QVBoxLayout(cell);
      cv->setContentsMargins(0, 0, 0, 0);
      cv->setSpacing(1);
      cv->addWidget(buildNode(fn, fp));
      cv->addWidget(helpLabel(fn, cell));
      g->addWidget(cell, row++, 1);
    }
  }
  const ote::Json* cur = ctx_->at(path);
  if (cur && cur->isObject()) {
    QWidget* other = buildOtherKeys(n, path);
    if (other) g->addWidget(other, row++, 0, 1, 2);
  }
  return w;
}

QWidget* FormView::buildBlock(const SchemaNode& n, const JsonPath& path) {
  auto* frame = new QFrame;
  frame->setObjectName("blockFrame");
  frame->setFrameShape(QFrame::StyledPanel);
  auto* v = new QVBoxLayout(frame);
  v->setContentsMargins(8, 6, 8, 8);
  const std::string keyStd = path.back().key;
  auto* head = new QCheckBox(QString::fromStdString(keyStd), frame);
  head->setObjectName("blockEnable");
  QFont f = head->font();
  f.setBold(true);
  head->setFont(f);
  head->setToolTip(tr("Enable: adds this optional block to the file. Untick: removes it (undo brings it back)."));
  const ote::Json* cur = ctx_->at(path);
  head->setChecked(cur != nullptr);
  v->addWidget(head);
  v->addWidget(helpLabel(n, frame));
  connect(head, &QCheckBox::clicked, this, [this, &n, path](bool on) {
    if (on) ctx_->set(path, defaultValue(n, ctx_->lookup()), tr("Enable %1").arg(pathToString(path)));
    else ctx_->erase(path);
  });
  if (cur) v->addWidget(buildObjectBody(n, path));
  return frame;
}

QWidget* FormView::buildList(const SchemaNode& n, const JsonPath& path) {
  auto* w = new QWidget;
  w->setObjectName("listField");
  w->setProperty("path", pathToString(path));  // tests (and the scripted UI test) find a list by its path
  auto* v = new QVBoxLayout(w);
  v->setContentsMargins(0, 0, 0, 0);
  v->setSpacing(4);
  const ote::Json* list = ctx_->at(path);
  const size_t count = list ? list->size() : 0;
  if (!list) {
    auto* none = new QLabel(tr("(not set)"), w);
    none->setForegroundRole(QPalette::PlaceholderText);
    v->addWidget(none);
  }
  const SchemaNode& of = *n.of;
  // A form with hundreds of unit entries would be slow to build and useless to scroll: show the first ones, point at the tree.
  constexpr size_t kMaxShown = 60;
  if (count > kMaxShown) {
    auto* more = new QLabel(tr("Showing the first %1 of %2 items; the tree view shows all of them.").arg(kMaxShown).arg(count), w);
    more->setObjectName("listTruncated");
    more->setStyleSheet(warningStyle());
    v->addWidget(more);
  }
  for (size_t i = 0; i < std::min(count, kMaxShown); ++i) {
    const JsonPath ip = child(path, i);
    auto* row = new QFrame(w);
    row->setObjectName("listItem");
    auto* rv = new QVBoxLayout(row);
    rv->setContentsMargins(0, 0, 0, 0);
    rv->setSpacing(2);
    auto* bar = new QHBoxLayout;
    bar->setContentsMargins(0, 0, 0, 0);
    const bool compound = !of.isScalar();
    if (compound) {
      row->setFrameShape(QFrame::StyledPanel);
      rv->setContentsMargins(6, 3, 6, 6);
      const QString s = summaryOf((*list)[i]);
      auto* title = new QLabel(QStringLiteral("[%1]  %2").arg(i + 1).arg(s), row);
      title->setObjectName("itemTitle");
      QFont f = title->font();
      f.setBold(true);
      title->setFont(f);
      bar->addWidget(title, 1);
    }
    auto* up = smallButton(QStringLiteral("▲"), tr("Move up"), row, "itemUp");
    auto* down = smallButton(QStringLiteral("▼"), tr("Move down"), row, "itemDown");
    auto* del = smallButton(QStringLiteral("×"), tr("Remove this item"), row, "itemRemove");
    up->setEnabled(i > 0);
    down->setEnabled(i + 1 < count);
    connect(up, &QToolButton::clicked, this, [this, path, i] { ctx_->moveItem(path, i, i - 1); });
    connect(down, &QToolButton::clicked, this, [this, path, i] { ctx_->moveItem(path, i, i + 1); });
    connect(del, &QToolButton::clicked, this, [this, ip] { ctx_->erase(ip); });
    if (compound) {
      bar->addWidget(up);
      bar->addWidget(down);
      bar->addWidget(del);
      rv->addLayout(bar);
      rv->addWidget(buildNode(of, ip, true));
    } else {
      bar->addWidget(buildNode(of, ip, true), 1);
      bar->addWidget(up);
      bar->addWidget(down);
      bar->addWidget(del);
      rv->addLayout(bar);
    }
    v->addWidget(row);
  }
  auto* add = new QPushButton(tr("Add item"), w);
  add->setObjectName("addItem");
  add->setToolTip(tr("Add a new item at the end of %1").arg(pathToString(path)));
  connect(add, &QPushButton::clicked, this, [this, &n, path] {
    ote::Json item = defaultValue(*n.of, ctx_->lookup());
    if (item.isNull()) item = ote::Json::makeObject();
    ctx_->appendItem(path, item);
  });
  auto* ab = new QHBoxLayout;
  ab->setContentsMargins(0, 0, 0, 0);
  ab->addWidget(add);
  ab->addStretch(1);
  v->addLayout(ab);
  return w;
}

QWidget* FormView::buildMap(const SchemaNode& n, const JsonPath& path) {
  auto* w = new QWidget;
  w->setObjectName("mapField");
  auto* g = new QGridLayout(w);
  g->setContentsMargins(0, 0, 0, 0);
  g->setHorizontalSpacing(10);
  g->setVerticalSpacing(3);
  g->setColumnMinimumWidth(0, 90);
  g->setColumnStretch(1, 1);
  const QStringList ids = ctx_->ids() ? ctx_->ids()->ids(n.refs) : QStringList();
  const ote::Json* cur = ctx_->at(path);
  QStringList rows = ids;
  QStringList unknown;
  if (cur && cur->isObject())
    for (const auto& m : cur->members()) {
      const QString k = QString::fromStdString(m.first);
      if (!ids.contains(k)) unknown << k;
    }
  int r = 0;
  auto addRow = [&](const QString& id, bool isUnknown) {
    auto* lab = new QLabel(isUnknown ? tr("%1 (unknown id)").arg(id) : id, w);
    lab->setObjectName("mapKey");
    if (isUnknown) lab->setStyleSheet(warningStyle());
    g->addWidget(lab, r, 0);
    g->addWidget(buildNode(*n.of, child(path, id.toStdString())), r, 1);
    ++r;
  };
  for (const QString& id : rows) addRow(id, false);
  for (const QString& id : unknown) addRow(id, true);
  if (rows.isEmpty() && unknown.isEmpty()) {
    auto* none = new QLabel(tr("No ids to list yet: %1 is empty or missing.").arg(QString::fromStdString(n.refs.front())), w);
    none->setForegroundRole(QPalette::PlaceholderText);
    g->addWidget(none, 0, 0, 1, 2);
  }
  if (ctx_->ids())
    connect(ctx_->ids(), &IdSource::idsChanged, w, [this, &n, ids] {
      if (ctx_->ids()->ids(n.refs) != ids) requestRebuild();
    });
  return w;
}

QWidget* FormView::buildOtherKeys(const SchemaNode& n, const JsonPath& path) {
  const ote::Json* obj = ctx_->at(path);
  ote::Json other = ote::Json::makeObject();
  for (const auto& m : obj->members())
    if (!n.field(m.first)) other.set(m.first, m.second);
  if (other.size() == 0) return nullptr;

  auto* box = new QFrame;
  box->setObjectName("otherKeys");
  box->setFrameShape(QFrame::StyledPanel);
  auto* v = new QVBoxLayout(box);
  v->setContentsMargins(6, 4, 6, 6);
  auto* toggle = new QToolButton(box);
  toggle->setObjectName("otherKeysToggle");
  toggle->setToolButtonStyle(Qt::ToolButtonTextOnly);
  toggle->setAutoRaise(true);
  toggle->setCheckable(true);
  const QString title = tr("Other keys (%1)").arg(other.size());
  toggle->setToolTip(tr("Keys that are in the file but not described by the schema. They are kept exactly as they are."));
  toggle->setText(QStringLiteral("▸ ") + title);
  v->addWidget(toggle);
  auto* body = new QWidget(box);
  auto* bv = new QVBoxLayout(body);
  bv->setContentsMargins(0, 0, 0, 0);
  auto* why = new QLabel(tr("In the file, not described by the schema. Kept exactly as they are."), body);
  why->setWordWrap(true);
  why->setForegroundRole(QPalette::PlaceholderText);
  bv->addWidget(why);
  auto* text = new QPlainTextEdit(body);
  text->setObjectName("otherKeysText");
  text->setReadOnly(true);
  text->setPlainText(QString::fromStdString(other.dump(2)));
  text->setMaximumHeight(130);
  QFont mono = text->font();
  mono.setStyleHint(QFont::Monospace);
  mono.setFamily("monospace");
  text->setFont(mono);
  bv->addWidget(text);
  auto* buttons = new QHBoxLayout;
  auto* edit = new QPushButton(tr("Edit as JSON..."), body);
  edit->setObjectName("otherKeysEdit");
  auto* tree = new QPushButton(tr("Show in tree view"), body);
  tree->setObjectName("otherKeysTree");
  buttons->addWidget(edit);
  buttons->addWidget(tree);
  buttons->addStretch(1);
  bv->addLayout(buttons);
  body->hide();
  v->addWidget(body);
  connect(toggle, &QToolButton::toggled, this, [toggle, body, title](bool on) {
    body->setVisible(on);
    toggle->setText((on ? QStringLiteral("▾ ") : QStringLiteral("▸ ")) + title);
  });
  connect(tree, &QPushButton::clicked, this, [this, path] { emit showInTreeRequested(path); });
  connect(edit, &QPushButton::clicked, this, [this, &n, path, other] {
    ote::Json result;
    if (!ctx_->editJson(tr("Other keys of %1").arg(path.empty() ? QStringLiteral("$") : pathToString(path)), other, &result, this)) return;
    if (!result.isObject()) return;
    QUndoStack* stack = ctx_->project()->undoStack();
    stack->beginMacro(tr("Edit other keys of %1").arg(pathToString(path)));
    for (const auto& m : other.members())
      if (!result.has(m.first.c_str())) ctx_->erase(child(path, m.first));
    for (const auto& m : result.members()) ctx_->set(child(path, m.first), m.second);
    stack->endMacro();
    (void)n;
  });
  anchors_.insert(pathToString(path) + "#other", box);
  return box;
}

}  // namespace ote_native
