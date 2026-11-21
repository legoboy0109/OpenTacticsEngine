// Building the widgets of a form from schema nodes: leaves, objects (one or two columns, fields that apply only for some
// `kind`), optional blocks, lists, maps, variants (one of several shapes). Containers are built lazily, see form_view.h.
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>
#include <algorithm>

#include "form_parts.h"
#include "form_view.h"
#include "project.h"

namespace ote_native {

// --- leaves ------------------------------------------------------------------------------------------------------

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
    else if (n.kind == K::Variant) expected = tr("one of the known shapes (%1)").arg([&] {
      QStringList names;
      for (const auto& vr : n.variants) names << QString::fromStdString(vr.name);
      return names.join(QStringLiteral(", "));
    }());
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

QWidget* FormView::buildNode(const SchemaNode& node, const JsonPath& path, bool inList) {
  using K = SchemaNode::Kind;
  const SchemaNode& n = node.resolved();  // a placeholder of a recursive definition (commands inside `if`) expands here
  ++built_;
  const ote::Json* v = ctx_->at(path);
  QWidget* w;
  if (v && isAlt(n, *v)) w = new JsonField(ctx_, this, &n, path, nullptr);  // the loader's other accepted form, as JSON
  else if (v && !typeOk(n, *v)) w = buildLeaf(n, path);
  else if (n.kind == K::Variant) w = buildVariant(n, path);
  else if (n.kind == K::List && n.length < 0) w = buildList(n, path);
  else if (n.kind == K::Map) w = n.free ? buildFreeMap(n, path) : buildMap(n, path);
  else if (n.kind == K::Object) w = n.block ? buildBlock(n, path) : buildObjectBody(n, path);
  else w = buildLeaf(n, path);
  if (inList && n.isScalar())
    if (auto* b = w->findChild<QToolButton*>("clearField")) b->hide();  // the list's own remove button does that job
  if (isCompound(n)) anchors_.insert(pathToString(path), w);
  return w;
}

// --- lazy containers --------------------------------------------------------------------------------------------

QWidget* FormView::lazyBody(const JsonPath& path, QBoxLayout* header, const std::function<QWidget*()>& make, bool startClosed) {
  const QString key = pathToString(path);
  const bool open = open_.contains(key) || (!closed_.contains(key) && !startClosed && built_ < kBudget);
  auto* toggle = new QToolButton;
  toggle->setObjectName("collapseToggle");
  toggle->setAutoRaise(true);
  toggle->setCheckable(true);
  toggle->setToolTip(tr("Show or hide the contents"));
  header->insertWidget(0, toggle);
  auto* holder = new QWidget;
  holder->setObjectName("lazyBody");
  auto* hl = new QVBoxLayout(holder);
  hl->setContentsMargins(0, 0, 0, 0);
  const int depth = bodyDepth_;
  auto build = [this, holder, make, depth] {
    if (holder->layout()->count() != 0) return;
    const int saved = bodyDepth_;
    bodyDepth_ = depth;
    holder->layout()->addWidget(make());
    bodyDepth_ = saved;
  };
  lazyOpen_[key] = open;
  toggle->setChecked(open);
  toggle->setText(open ? QStringLiteral("▾") : QStringLiteral("▸"));
  if (open) build();
  else {
    holder->hide();
    expanders_.insert(key, [toggle] { toggle->setChecked(true); });
  }
  connect(toggle, &QToolButton::toggled, toggle, [this, toggle, holder, build, key](bool on) {
    toggle->setText(on ? QStringLiteral("▾") : QStringLiteral("▸"));
    lazyOpen_[key] = on;
    if (on) {
      build();
      expanders_.remove(key);
      open_.insert(key);
      closed_.remove(key);
    } else {
      closed_.insert(key);
      open_.remove(key);
      // A closed container can be opened again from focus requests: re-register its opener.
      expanders_.insert(key, [toggle] { toggle->setChecked(true); });
    }
    holder->setVisible(on);
  });
  return holder;
}

// --- objects ----------------------------------------------------------------------------------------------------

QWidget* FormView::buildObjectBody(const SchemaNode& n, const JsonPath& path) {
  ++bodyDepth_;
  auto* w = new QWidget;
  w->setObjectName("objectBody");
  auto* g = new QGridLayout(w);
  g->setContentsMargins(0, 0, 0, 0);
  g->setHorizontalSpacing(10);
  g->setVerticalSpacing(7);
  // Two columns of fields (label, field, label, field) when the form is wide and this object is near the top.
  const int cols = (columns_ == 2 && bodyDepth_ <= 2) ? 2 : 1;
  g->setColumnMinimumWidth(0, 120);
  g->setColumnStretch(1, 1);
  if (cols == 2) {
    g->setColumnMinimumWidth(2, 120);
    g->setColumnStretch(3, 1);
    g->setHorizontalSpacing(14);
  }
  const ote::Json* cur = ctx_->at(path);
  int row = 0, slot = 0;  // slot: scalar fields already placed in this row
  auto nextRow = [&] {
    if (slot > 0) ++row;
    slot = 0;
  };
  for (const auto& f : n.fields) {
    const SchemaNode& fn = f.second->resolved();
    const JsonPath fp = childPath(path, f.first);
    const QString key = QString::fromStdString(f.first);
    bool applies = true;
    if (fn.when) {
      discriminators_.insert(pathToString(childPath(path, fn.when->key)));
      applies = fieldApplies(n, fn, cur);
      // A key that does not apply but is in the file is still shown (never hidden data), with a note.
      if (!applies && !(cur && cur->isObject() && cur->has(f.first.c_str()))) continue;
    }
    QLabel* notApplicable = nullptr;
    if (!applies) {
      QStringList words;
      for (const std::string& s : fn.when->values) words << QString::fromStdString(s);
      notApplicable = new QLabel(tr("Does not apply while %1 is not %2. The file keeps it as it is.").arg(QString::fromStdString(fn.when->key), words.join(tr(" or "))), w);
      notApplicable->setObjectName("notApplicable");
      notApplicable->setWordWrap(true);
      notApplicable->setStyleSheet(warningStyle());
    }
    if (fn.kind == SchemaNode::Kind::Object && fn.block) {  // an optional block draws its own header (the Enable checkbox)
      nextRow();
      g->addWidget(buildNode(fn, fp), row++, 0, 1, cols * 2);
      if (notApplicable) g->addWidget(notApplicable, row++, 0, 1, cols * 2);
      continue;
    }
    if (isCompound(fn)) {
      // Lists, maps, sub-objects get their own full-width row under the key and its help.
      nextRow();
      auto* head = new QWidget(w);
      auto* hv = new QVBoxLayout(head);
      hv->setContentsMargins(0, 4, 0, 0);
      hv->setSpacing(0);
      {
        auto* top = new QHBoxLayout;
        top->setContentsMargins(0, 0, 0, 0);
        top->addWidget(keyLabel(key, fn.required, head));
        top->addStretch(1);
        const ote::Json* cv = ctx_->at(fp);
        if (cv && !fn.required && fn.kind != SchemaNode::Kind::Object) {
          auto* x = smallButton(QStringLiteral("×"), tr("Remove this key from the file"), head, "clearField");
          connect(x, &QToolButton::clicked, this, [this, fp] { ctx_->erase(fp); });
          top->addWidget(x);
        }
        hv->addLayout(top);
        hv->addWidget(helpLabel(fn, head));
        if (notApplicable) hv->addWidget(notApplicable);
      }
      g->addWidget(head, row++, 0, 1, cols * 2);
      QWidget* content = buildNode(fn, fp);
      auto* indent = new QWidget(w);
      auto* il = new QHBoxLayout(indent);
      il->setContentsMargins(14, 0, 0, 0);
      il->addWidget(content);
      g->addWidget(indent, row++, 0, 1, cols * 2);
    } else {
      const int c0 = slot * 2;
      g->addWidget(keyLabel(key, fn.required, w), row, c0, Qt::AlignTop);
      auto* cell = new QWidget(w);
      auto* cv = new QVBoxLayout(cell);
      cv->setContentsMargins(0, 0, 0, 0);
      cv->setSpacing(1);
      cv->addWidget(buildNode(fn, fp));
      cv->addWidget(helpLabel(fn, cell));
      if (notApplicable) cv->addWidget(notApplicable);
      g->addWidget(cell, row, c0 + 1);
      if (++slot == cols) nextRow();
    }
  }
  nextRow();
  if (cur && cur->isObject()) {
    QWidget* other = buildOtherKeys(n, path);
    if (other) g->addWidget(other, row++, 0, 1, cols * 2);
  }
  --bodyDepth_;
  return w;
}

QWidget* FormView::buildBlock(const SchemaNode& n, const JsonPath& path) {
  auto* frame = new QFrame;
  frame->setObjectName("blockFrame");
  frame->setFrameShape(QFrame::StyledPanel);
  auto* v = new QVBoxLayout(frame);
  v->setContentsMargins(8, 6, 8, 8);
  const std::string keyStd = path.back().key;
  auto* hb = new QHBoxLayout;
  hb->setContentsMargins(0, 0, 0, 0);
  auto* head = new QCheckBox(QString::fromStdString(keyStd), frame);
  head->setObjectName("blockEnable");
  QFont f = head->font();
  f.setBold(true);
  head->setFont(f);
  head->setToolTip(tr("Enable: adds this optional block to the file. Untick: removes it (undo brings it back)."));
  const ote::Json* cur = ctx_->at(path);
  head->setChecked(cur != nullptr);
  hb->addWidget(head, 1);
  v->addLayout(hb);
  v->addWidget(helpLabel(n, frame));
  // A block that cannot be valid on its own yet (it names a resource and the game has none) says what to add first,
  // instead of writing something the engine would refuse.
  if (!cur && n.needs && ctx_->idsOf(n.needs->refs).isEmpty()) {
    head->setEnabled(false);
    auto* why = new QLabel(QString::fromStdString(n.needs->hint), frame);
    why->setObjectName("blockNeeds");
    why->setWordWrap(true);
    why->setStyleSheet(warningStyle());
    v->addWidget(why);
  }
  connect(head, &QCheckBox::clicked, this, [this, &n, path](bool on) {
    if (on) ctx_->set(path, defaultValue(n, ctx_->lookup()), tr("Enable %1").arg(pathToString(path)));
    else ctx_->erase(path);
  });
  if (cur) v->addWidget(lazyBody(path, hb, [this, &n, path] { return buildObjectBody(n, path); }));
  return frame;
}

// --- variants -----------------------------------------------------------------------------------------------------

QWidget* FormView::buildVariant(const SchemaNode& n, const JsonPath& path) {
  auto* frame = new QWidget;
  frame->setObjectName("variantField");
  auto* v = new QVBoxLayout(frame);
  v->setContentsMargins(0, 0, 0, 0);
  v->setSpacing(3);
  const ote::Json* cur = ctx_->at(path);
  const int idx = cur ? variantIndex(n, *cur) : -1;
  for (const auto& vr : n.variants)
    if (!vr.matchKey.empty()) discriminators_.insert(pathToString(childPath(path, vr.matchKey)));
  auto* combo = new QComboBox(frame);
  combo->setObjectName("variantCombo");
  combo->setToolTip(tr("Which form this value takes. Switching replaces the value (undo brings it back); keys both forms have are kept."));
  if (!cur) combo->addItem(tr("(not set: choose)"), -1);
  for (size_t i = 0; i < n.variants.size(); ++i) combo->addItem(QString::fromStdString(n.variants[i].name), (int)i);
  combo->setCurrentIndex(combo->findData(idx));
  auto* row = new QHBoxLayout;
  row->setContentsMargins(0, 0, 0, 0);
  row->addWidget(combo);
  row->addStretch(1);
  v->addLayout(row);
  connect(combo, &QComboBox::activated, this, [this, &n, path, combo](int r) {
    const int i = combo->itemData(r).toInt();
    if (i < 0 || (size_t)i >= n.variants.size()) return;
    ote::Json value = defaultValueOfVariant(n.variants[(size_t)i], ctx_->lookup());
    const ote::Json* old = ctx_->at(path);
    const SchemaNode* shape = n.variants[(size_t)i].node ? &n.variants[(size_t)i].node->resolved() : nullptr;
    if (old && old->isObject() && value.isObject() && shape)  // keep the keys the new shape shares (`who` of a condition)
      for (const auto& m : old->members())
        if (!value.has(m.first.c_str()))
          if (const SchemaNode* fld = shape->field(m.first))
            if (typeOk(*fld, m.second)) value.set(m.first, m.second);
    ctx_->setField(n, path, value, tr("Change %1 to %2").arg(pathToString(path), QString::fromStdString(n.variants[(size_t)i].name)));
  });
  if (idx >= 0 && n.variants[(size_t)idx].node) v->addWidget(buildNode(*n.variants[(size_t)idx].node, path));
  return frame;
}

// --- lists ------------------------------------------------------------------------------------------------------

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
  const SchemaNode& of = n.of->resolved();
  {
    ListRows& lr = lists_[pathToString(path)];  // (an empty list takes drops too)
    lr.widget = w;
    lr.item = &of;
    lr.path = path;
  }
  // Long lists (a 300-command event) are built one page at a time: a form with hundreds of items would be slow to build and
  // useless to scroll. The page is remembered per list; opening a key on another page switches to it (expandAlong).
  const QString listKey = pathToString(path);
  const size_t pageCount = (count + kPageSize - 1) / kPageSize;
  size_t page = pages_.value(listKey, 0);
  if (pageCount > 0 && page >= pageCount) page = pageCount - 1;
  const size_t first = page * kPageSize, last = std::min(count, first + kPageSize);
  auto pager = [&](const char* name) {
    auto* bar = new QWidget(w);
    bar->setObjectName(name);
    auto* hb = new QHBoxLayout(bar);
    hb->setContentsMargins(0, 0, 0, 0);
    auto* prev = smallButton(QStringLiteral("◀"), tr("Previous page"), bar, "pagePrev");
    auto* next = smallButton(QStringLiteral("▶"), tr("Next page"), bar, "pageNext");
    auto* label = new QLabel(tr("Items %1 to %2 of %3").arg(first + 1).arg(last).arg(count), bar);
    label->setObjectName("pageLabel");
    prev->setEnabled(page > 0);
    next->setEnabled(page + 1 < pageCount);
    connect(prev, &QToolButton::clicked, this, [this, listKey, page] { pages_[listKey] = page - 1; requestRebuild(); });
    connect(next, &QToolButton::clicked, this, [this, listKey, page] { pages_[listKey] = page + 1; requestRebuild(); });
    hb->addWidget(prev);
    hb->addWidget(label);
    hb->addWidget(next);
    hb->addStretch(1);
    return bar;
  };
  if (pageCount > 1) v->addWidget(pager("pagerTop"));
  for (size_t i = first; i < last; ++i) {
    const JsonPath ip = childPath(path, i);
    auto* row = new QFrame(w);
    row->setObjectName("listItem");
    auto* rv = new QVBoxLayout(row);
    rv->setContentsMargins(0, 0, 0, 0);
    rv->setSpacing(2);
    auto* bar = new QHBoxLayout;
    bar->setContentsMargins(0, 0, 0, 0);
    bar->addWidget(makeDragHandle(path, i, row));  // (lazyBody puts the open / close arrow in front of it)
    registerListItem(path, i, row);
    const bool compound = !of.isScalar();
    if (compound) {
      row->setFrameShape(QFrame::StyledPanel);
      rv->setContentsMargins(6, 3, 6, 6);
      const ote::Json& item = (*list)[i];
      const QString s = summaryOf(of, item);
      // A variant item reads "[3]  say   Eirik: "The ford ..."": the raw key in bold, what it does after it.
      const int vi = of.kind == SchemaNode::Kind::Variant ? variantIndex(of, item) : -1;
      const bool named = vi >= 0 && of.variants[(size_t)vi].summary.size() > 0;
      auto* title = new QLabel(named ? QStringLiteral("[%1]  %2").arg(i + 1).arg(QString::fromStdString(of.variants[(size_t)vi].name))
                                     : QStringLiteral("[%1]  %2").arg(i + 1).arg(s),
                               row);
      title->setObjectName("itemTitle");
      QFont f = title->font();
      f.setBold(true);
      title->setFont(f);
      bar->addWidget(title, named ? 0 : 1);
      if (named) {
        auto* sum = new ElidedLabel(s, row);
        sum->setObjectName("itemSummary");
        sum->setToolTip(s);
        bar->addWidget(sum, 1);
      }
    }
    auto* up = smallButton(QStringLiteral("▲"), tr("Move up"), row, "itemUp");
    auto* down = smallButton(QStringLiteral("▼"), tr("Move down"), row, "itemDown");
    auto* dup = smallButton(QStringLiteral("⧉"), tr("Duplicate this item (the copy goes right after it)"), row, "itemDuplicate");
    auto* del = smallButton(QStringLiteral("×"), tr("Remove this item"), row, "itemRemove");
    up->setEnabled(i > 0);
    down->setEnabled(i + 1 < count);
    connect(up, &QToolButton::clicked, this, [this, path, i] { moveListItem(path, i, path, i - 1, QStringLiteral("itemUp")); });
    connect(down, &QToolButton::clicked, this, [this, path, i] { moveListItem(path, i, path, i + 2, QStringLiteral("itemDown")); });
    connect(dup, &QToolButton::clicked, this, [this, path, i] {
      if (const ote::Json* cur = ctx_->at(childPath(path, i))) ctx_->insertItem(path, i + 1, *cur);
    });
    connect(del, &QToolButton::clicked, this, [this, ip] { ctx_->erase(ip); });
    if (compound) {
      bar->addWidget(up);
      bar->addWidget(down);
      bar->addWidget(dup);
      bar->addWidget(del);
      rv->addLayout(bar);
      rv->addWidget(lazyBody(ip, bar, [this, &of, ip] { return buildNode(of, ip, true); }, n.collapsed));
    } else {
      bar->addWidget(buildNode(of, ip, true), 1);
      bar->addWidget(up);
      bar->addWidget(down);
      bar->addWidget(dup);
      bar->addWidget(del);
      rv->addLayout(bar);
    }
    v->addWidget(row);
  }
  if (pageCount > 1) v->addWidget(pager("pagerBottom"));
  // A list of one-of-several shapes (commands, conditions) asks which kind first, with a line of help for each.
  const bool picks = of.kind == SchemaNode::Kind::Variant && of.variants.size() > 1;
  auto* add = new QPushButton(picks ? tr("Add...") : tr("Add item"), w);
  add->setObjectName("addItem");
  add->setToolTip(tr("Add a new item at the end of %1").arg(pathToString(path)));
  connect(add, &QPushButton::clicked, this, [this, &n, &of, path, picks, add] {
    ote::Json item;
    if (picks) {
      std::vector<std::pair<QString, QString>> entries;
      for (const auto& vr : of.variants) entries.emplace_back(QString::fromStdString(vr.name), QString::fromStdString(vr.help));
      const int r = ctx_->pickVariant(tr("Add to %1").arg(path.empty() || path.back().isIndex ? pathToString(path) : QString::fromStdString(path.back().key)), entries, add);
      if (r < 0) return;
      item = defaultValueOfVariant(of.variants[(size_t)r], ctx_->lookup());
    } else {
      item = defaultValue(n.of->resolved(), ctx_->lookup());
      if (item.isNull()) item = ote::Json::makeObject();
    }
    ctx_->appendItem(path, item);
    // Show the page the new item is on, and open it: somebody who just added a command wants to fill it in.
    const size_t countNow = ctx_->at(path) ? ctx_->at(path)->size() : 0;
    if (countNow > 0) {
      pages_[pathToString(path)] = (countNow - 1) / kPageSize;
      open_.insert(pathToString(childPath(path, countNow - 1)));
      closed_.remove(pathToString(childPath(path, countNow - 1)));
    }
  });
  auto* ab = new QHBoxLayout;
  ab->setContentsMargins(0, 0, 0, 0);
  ab->addWidget(add);
  ab->addStretch(1);
  v->addLayout(ab);
  return w;
}

// --- maps -------------------------------------------------------------------------------------------------------

QWidget* FormView::buildMap(const SchemaNode& n, const JsonPath& path) {
  auto* w = new QWidget;
  w->setObjectName("mapField");
  auto* g = new QGridLayout(w);
  g->setContentsMargins(0, 0, 0, 0);
  g->setHorizontalSpacing(10);
  g->setVerticalSpacing(3);
  g->setColumnMinimumWidth(0, 90);
  g->setColumnStretch(1, 1);
  const QStringList ids = ctx_->idsOf(n.refs);
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
    g->addWidget(buildNode(*n.of, childPath(path, id.toStdString())), r, 1);  // (buildNode resolves a placeholder)
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
      if (ctx_->idsOf(n.refs) != ids) requestRebuild();
    });
  return w;
}

// A map whose keys are the user's own names (named AI weight sets, numbers a formula reads): one row per key with a remove
// button, and a name box with Add under them.
QWidget* FormView::buildFreeMap(const SchemaNode& n, const JsonPath& path) {
  auto* w = new QWidget;
  w->setObjectName("mapField");
  w->setProperty("free", true);
  auto* v = new QVBoxLayout(w);
  v->setContentsMargins(0, 0, 0, 0);
  v->setSpacing(3);
  const ote::Json* cur = ctx_->at(path);
  const bool scalarValues = n.of->resolved().isScalar();
  if (cur && cur->isObject())
    for (const auto& m : cur->members()) {
      const std::string key = m.first;
      auto* row = new QWidget(w);
      row->setObjectName("freeMapRow");
      auto* hl = new QHBoxLayout(row);
      hl->setContentsMargins(0, 0, 0, 0);
      auto* lab = new QLabel(QString::fromStdString(key), row);
      lab->setObjectName("mapKey");
      QFont f = lab->font();
      f.setBold(true);
      lab->setFont(f);
      auto* del = smallButton(QStringLiteral("×"), tr("Remove this entry"), row, "mapRemove");
      connect(del, &QToolButton::clicked, this, [this, path, key] { ctx_->erase(childPath(path, key)); });
      if (scalarValues) {
        lab->setMinimumWidth(110);
        hl->addWidget(lab);
        hl->addWidget(buildNode(*n.of, childPath(path, key), true), 1);
        hl->addWidget(del);
        v->addWidget(row);
      } else {
        auto* frame = new QFrame(w);
        frame->setFrameShape(QFrame::StyledPanel);
        auto* fl = new QVBoxLayout(frame);
        fl->setContentsMargins(6, 3, 6, 6);
        hl->addWidget(lab, 1);
        hl->addWidget(del);
        fl->addWidget(row);
        fl->addWidget(lazyBody(childPath(path, key), hl, [this, &n, path, key] { return buildNode(*n.of, childPath(path, key)); }));
        v->addWidget(frame);
      }
    }
  auto* add = new QHBoxLayout;
  add->setContentsMargins(0, 0, 0, 0);
  auto* name = new QLineEdit(w);
  name->setObjectName("mapNewKey");
  name->setPlaceholderText(tr("name of a new entry"));
  name->setMaximumWidth(220);
  auto* addB = new QPushButton(tr("Add entry"), w);
  addB->setObjectName("mapAdd");
  auto* note = new QLabel(w);
  note->setObjectName("fieldNote");
  note->setStyleSheet(warningStyle());
  note->hide();
  auto doAdd = [this, &n, path, name, note] {
    const QString k = name->text().trimmed();
    if (k.isEmpty()) return;
    const ote::Json* c = ctx_->at(path);
    if (c && c->isObject() && c->has(k.toStdString().c_str())) {
      note->setText(tr("\"%1\" is already there.").arg(k));
      note->show();
      return;
    }
    ote::Json item = defaultValue(*n.of, ctx_->lookup());
    if (item.isNull()) item = ote::Json::makeObject();
    ctx_->set(childPath(path, k.toStdString()), item, tr("Add %1").arg(k));
  };
  connect(addB, &QPushButton::clicked, this, doAdd);
  connect(name, &QLineEdit::returnPressed, this, doAdd);
  add->addWidget(name);
  add->addWidget(addB);
  add->addStretch(1);
  v->addLayout(add);
  v->addWidget(note);
  return w;
}

}  // namespace ote_native
