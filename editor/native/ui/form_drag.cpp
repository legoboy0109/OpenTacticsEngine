// Moving list items in a form: the grip (DragHandle), the picture of a drag in progress (a floating copy of the item's title, a
// line where it would land, the item itself dimmed), the rules for where an item may go, and the bookkeeping that keeps the
// form's per-path memory (which items are open, which page of a long list is shown, which field has the focus) in step with the
// file when items change places.
//
// Rules: an item moves within its own list, or into another list of this file whose items are of the same kind (the same
// command variants, the same object keys): from one event's commands into another's, into an `if` branch. Plain value lists
// (ids, numbers) reorder only. An item never goes into itself or into something inside it.
#include <QApplication>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>

#include "form_drag.h"
#include "form_parts.h"
#include "form_view.h"

namespace ote_native {

// --- the grip ----------------------------------------------------------------------------------------------------

DragHandle::DragHandle(QWidget* parent) : QToolButton(parent) {
  setObjectName("dragHandle");
  setText(QStringLiteral("≡"));
  setAutoRaise(true);
  setCursor(Qt::OpenHandCursor);
  setFocusPolicy(Qt::StrongFocus);
  setToolTip(tr("Drag to move this item, also into another list of the same kind. With the keyboard: Alt+Up / Alt+Down."));
}

void DragHandle::mousePressEvent(QMouseEvent* e) {
  if (e->button() != Qt::LeftButton) return QToolButton::mousePressEvent(e);
  pressed_ = true;
  dragging_ = false;
  pressAt_ = e->globalPosition().toPoint();
  setFocus(Qt::MouseFocusReason);
  e->accept();
}

void DragHandle::mouseMoveEvent(QMouseEvent* e) {
  if (!pressed_) return QToolButton::mouseMoveEvent(e);
  const QPoint g = e->globalPosition().toPoint();
  if (!dragging_) {
    if ((g - pressAt_).manhattanLength() < QApplication::startDragDistance()) return;
    dragging_ = true;
    emit dragStarted(pressAt_);
  }
  emit dragMoved(g);
  e->accept();
}

void DragHandle::mouseReleaseEvent(QMouseEvent* e) {
  if (e->button() != Qt::LeftButton) return QToolButton::mouseReleaseEvent(e);
  if (!pressed_) {  // the drag was cancelled with Escape while the button was still down
    e->accept();
    return;
  }
  const bool was = dragging_;
  pressed_ = dragging_ = false;
  if (was) emit dragDropped(e->globalPosition().toPoint());
  e->accept();
}

void DragHandle::keyPressEvent(QKeyEvent* e) {
  if (dragging_ && e->key() == Qt::Key_Escape) {
    pressed_ = dragging_ = false;
    emit dragCancelled();
    e->accept();
    return;
  }
  if ((e->modifiers() & Qt::AltModifier) && (e->key() == Qt::Key_Up || e->key() == Qt::Key_Down)) {
    emit stepRequested(e->key() == Qt::Key_Up ? -1 : 1);
    e->accept();
    return;
  }
  QToolButton::keyPressEvent(e);
}

// --- what may go where -------------------------------------------------------------------------------------------

namespace {

// Two lists take each other's items when their items are described alike: the same variants (by name) or the same object keys.
QString shapeSignature(const SchemaNode& item) {
  const SchemaNode& n = item.resolved();
  QString s = QString::number((int)n.kind);
  if (n.kind == SchemaNode::Kind::Variant)
    for (const auto& v : n.variants) s += "|" + QString::fromStdString(v.name);
  else if (n.kind == SchemaNode::Kind::Object)
    for (const auto& f : n.fields) s += "|" + QString::fromStdString(f.first);
  return s;
}

bool movesAcrossLists(const SchemaNode& item) {
  const SchemaNode::Kind k = item.resolved().kind;
  return k == SchemaNode::Kind::Variant || k == SchemaNode::Kind::Object;
}

QString listPrefix(const JsonPath& list) { return list.empty() ? QStringLiteral("[") : pathToString(list) + "["; }

// "events[1].do[3].then[0]" with prefix "events[1].do[": the index after the prefix, when it is `atLeast` or more, becomes +delta.
void shiftIndex(QString& key, const QString& prefix, size_t atLeast, int delta) {
  if (!key.startsWith(prefix)) return;
  const qsizetype close = key.indexOf(']', prefix.size());
  if (close < 0) return;
  bool ok = false;
  const qulonglong idx = key.mid(prefix.size(), close - prefix.size()).toULongLong(&ok);
  if (!ok || idx < atLeast) return;
  key = prefix + QString::number((qlonglong)idx + delta) + key.mid(close);
}

bool underPath(const QString& key, const QString& base) { return key == base || key.startsWith(base + ".") || key.startsWith(base + "["); }

}  // namespace

// --- the drag in progress ----------------------------------------------------------------------------------------

struct FormView::DragState {
  JsonPath srcList;
  size_t from = 0;
  QString signature;
  bool across = false;
  QPointer<QLabel> ghost;
  QPointer<QFrame> line;
  QPointer<QFrame> srcRow;
  QTimer* scroll = nullptr;
  int scrollDir = 0;
  QPoint last;
  bool valid = false;  // there is a place the item would go
  JsonPath dstList;
  size_t dstIndex = 0;
};

QToolButton* FormView::makeDragHandle(const JsonPath& list, size_t index, QWidget* parent) {
  auto* h = new DragHandle(parent);
  h->setProperty("listPath", pathToString(list));
  h->setProperty("itemIndex", (qulonglong)index);
  connect(h, &DragHandle::dragStarted, this, [this, list, index](QPoint g) { beginDrag(list, index, g); });
  connect(h, &DragHandle::dragMoved, this, [this](QPoint g) { updateDrag(g); });
  connect(h, &DragHandle::dragDropped, this, [this](QPoint g) {
    updateDrag(g);
    endDrag(true);
  });
  connect(h, &DragHandle::dragCancelled, this, [this] { endDrag(false); });
  connect(h, &DragHandle::stepRequested, this, [this, list, index](int delta) {
    const ote::Json* l = ctx_->at(list);
    if (!l || (delta < 0 && index == 0) || (delta > 0 && index + 1 >= l->size())) return;
    moveListItem(list, index, list, delta < 0 ? index - 1 : index + 2, QStringLiteral("dragHandle"));
  });
  return h;
}

void FormView::registerListItem(const JsonPath& list, size_t index, QFrame* row) {
  ListRows& lr = lists_[pathToString(list)];
  lr.rows.append({index, QPointer<QFrame>(row)});
  itemRows_.insert(pathToString(childPath(list, index)), row);
}

QWidget* FormView::dragHandleAt(const JsonPath& list, size_t index) const {
  QFrame* row = itemRows_.value(pathToString(childPath(list, index))).data();
  return row ? row->findChild<DragHandle*>("dragHandle") : nullptr;
}

bool FormView::dropTarget(JsonPath* list, size_t* index) const {
  if (!drag_ || !drag_->valid) return false;
  if (list) *list = drag_->dstList;
  if (index) *index = drag_->dstIndex;
  return true;
}

void FormView::beginDrag(const JsonPath& list, size_t index, QPoint global) {
  endDrag(false);
  const ListRows lr = lists_.value(pathToString(list));
  QFrame* row = itemRows_.value(pathToString(childPath(list, index))).data();
  if (!lr.item || !row || !body_) return;
  drag_ = new DragState;
  DragState* d = drag_;
  d->srcList = list;
  d->from = index;
  d->signature = shapeSignature(*lr.item);
  d->across = movesAcrossLists(*lr.item);
  d->srcRow = row;
  QString title;
  if (auto* t = row->findChild<QLabel*>("itemTitle")) title = t->text();
  if (auto* s = row->findChild<QLabel*>("itemSummary")) title += "   " + s->text();
  if (title.isEmpty()) title = QStringLiteral("[%1]").arg(index + 1);
  d->ghost = new QLabel(title, viewport());
  d->ghost->setObjectName("dragGhost");
  d->ghost->setAttribute(Qt::WA_TransparentForMouseEvents);
  d->ghost->setStyleSheet("background: palette(base); color: palette(text); border: 1px solid palette(highlight); padding: 3px 10px;");
  d->ghost->setMaximumWidth(std::max(200, viewport()->width() - 40));
  d->ghost->adjustSize();
  d->ghost->show();
  d->line = new QFrame(body_);
  d->line->setObjectName("dropLine");
  d->line->setAttribute(Qt::WA_TransparentForMouseEvents);
  d->line->setStyleSheet("background: palette(highlight);");
  d->line->setFixedHeight(3);
  d->line->hide();
  auto* dim = new QGraphicsOpacityEffect(row);
  dim->setOpacity(0.4);
  row->setGraphicsEffect(dim);
  d->scroll = new QTimer(this);
  d->scroll->setInterval(30);
  connect(d->scroll, &QTimer::timeout, this, [this] {
    if (!drag_) return;
    verticalScrollBar()->setValue(verticalScrollBar()->value() + drag_->scrollDir * 18);
    updateDrag(drag_->last);
  });
  QApplication::setOverrideCursor(Qt::ClosedHandCursor);
  updateDrag(global);
}

void FormView::updateDrag(QPoint global) {
  if (!drag_) return;
  DragState* d = drag_;
  d->last = global;
  const QPoint inView = viewport()->mapFromGlobal(global);
  if (d->ghost) {
    d->ghost->move(inView + QPoint(14, 10));
    d->ghost->raise();
  }
  // Near the top or bottom edge the form scrolls on its own, so a long list can be crossed in one drag.
  const int dir = inView.y() < 28 ? -1 : inView.y() > viewport()->height() - 28 ? 1 : 0;
  d->scrollDir = dir;
  if (dir != 0 && !d->scroll->isActive()) d->scroll->start();
  if (dir == 0) d->scroll->stop();

  // The innermost list under the pointer that takes this item.
  const JsonPath srcItem = childPath(d->srcList, d->from);
  const ListRows* best = nullptr;
  qint64 bestArea = 0;
  for (auto it = lists_.cbegin(); it != lists_.cend(); ++it) {
    const ListRows& lr = it.value();
    if (!lr.widget || !lr.item || !lr.widget->isVisible()) continue;
    if (lr.path != d->srcList) {
      if (!d->across || shapeSignature(*lr.item) != d->signature || pathStartsWith(lr.path, srcItem)) continue;
    }
    const QRect r(lr.widget->mapToGlobal(QPoint(0, 0)), lr.widget->size());
    if (!r.contains(global)) continue;
    const qint64 area = (qint64)r.width() * r.height();
    if (!best || area < bestArea) {
      best = &lr;
      bestArea = area;
    }
  }
  d->valid = false;
  if (d->line) d->line->hide();
  if (!best) return;
  // Before the first row whose middle is below the pointer, else after the last row.
  size_t index = 0;
  int y = 0;
  const QPoint origin = best->widget->mapTo(body_, QPoint(0, 0));
  bool placed = false;
  QFrame* lastRow = nullptr;
  for (const auto& rw : best->rows) {
    QFrame* row = rw.second.data();
    if (!row) continue;
    lastRow = row;
    const int mid = row->mapToGlobal(QPoint(0, row->height() / 2)).y();
    if (global.y() < mid) {
      index = rw.first;
      y = row->mapTo(body_, QPoint(0, 0)).y() - 2;
      placed = true;
      break;
    }
    index = rw.first + 1;
  }
  if (!placed) y = lastRow ? lastRow->mapTo(body_, QPoint(0, lastRow->height())).y() + 1 : origin.y();
  if (best->path == d->srcList && (index == d->from || index == d->from + 1)) return;  // where it already is
  d->valid = true;
  d->dstList = best->path;
  d->dstIndex = index;
  if (d->line) {
    d->line->setGeometry(origin.x(), y, best->widget->width(), 3);
    d->line->show();
    d->line->raise();
  }
}

void FormView::endDrag(bool drop) {
  if (!drag_) return;
  DragState* d = drag_;
  drag_ = nullptr;
  const bool go = drop && d->valid;
  const JsonPath srcList = d->srcList, dstList = d->dstList;
  const size_t from = d->from, to = d->dstIndex;
  delete d->ghost.data();
  delete d->line.data();
  if (d->srcRow) d->srcRow->setGraphicsEffect(nullptr);  // (deletes the dimming effect)
  delete d->scroll;
  delete d;
  QApplication::restoreOverrideCursor();
  if (go) moveListItem(srcList, from, dstList, to, QStringLiteral("dragHandle"));
}

// --- moving, with the form's memory kept in step ---------------------------------------------------------------

bool FormView::moveListItem(const JsonPath& srcList, size_t from, const JsonPath& dstList, size_t to, const QString& focusButton) {
  const QString focusKey = currentFocusKey();
  // Items that are open stay open: record the state of everything below the moved item before the paths change (an item that
  // is open only because the form had room for it is not in open_ yet).
  const QString itemStr = pathToString(childPath(srcList, from));
  for (auto it = lazyOpen_.cbegin(); it != lazyOpen_.cend(); ++it)
    if (underPath(it.key(), itemStr)) {
      (it.value() ? open_ : closed_).insert(it.key());
      (it.value() ? closed_ : open_).remove(it.key());
    }
  FormContext::MoveInfo mi;
  if (!ctx_->transferItem(srcList, from, dstList, to, &mi)) return false;

  // The same two steps the file went through: the insert shifts what is below it, the erase takes the original out (its
  // subtree's state goes to the new place) and shifts what was below that.
  const QString insertPrefix = listPrefix(mi.insertList);
  const QString erasedStr = pathToString(mi.erased), movedStr = pathToString(mi.moved);
  const QString erasePrefix = listPrefix(JsonPath(mi.erased.begin(), mi.erased.end() - 1));
  const size_t eraseIdx = mi.erased.back().index;
  auto mapKey = [&](QString key) {
    shiftIndex(key, insertPrefix, mi.insertAt, +1);
    if (underPath(key, erasedStr)) return movedStr + key.mid(erasedStr.size());
    shiftIndex(key, erasePrefix, eraseIdx + 1, -1);
    return key;
  };
  QSet<QString> open, closed;
  for (const QString& k : open_) open.insert(mapKey(k));
  for (const QString& k : closed_) closed.insert(mapKey(k));
  QHash<QString, size_t> pages;
  for (auto it = pages_.cbegin(); it != pages_.cend(); ++it) pages.insert(mapKey(it.key()), it.value());
  open_ = open;
  closed_ = closed;
  pages_ = pages;
  // Show the page the item landed on; give the focus back to the field it was in, or to the grip / button that moved it.
  pages_[pathToString(JsonPath(mi.moved.begin(), mi.moved.end() - 1))] = mi.moved.back().index / kPageSize;
  pendingFocusKey_ = focusKey.isEmpty() ? QString() : mapKey(focusKey);
  pendingFocusItem_ = focusKey.isEmpty() ? movedStr : QString();
  pendingFocusButton_ = focusButton;
  return true;
}

}  // namespace ote_native
