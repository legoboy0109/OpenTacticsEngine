#include "form_view.h"

#include <QApplication>
#include <QFrame>
#include <QLabel>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

#include "form_parts.h"
#include "project.h"

namespace ote_native {

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

void FormView::resizeEvent(QResizeEvent* e) {
  QScrollArea::resizeEvent(e);
  // Two columns of fields from kWideForm pixels on; crossing the line rebuilds the form (cheap: it is lazy).
  const int want = e->size().width() >= kWideForm ? 2 : 1;
  if (want != columns_) {
    columns_ = want;
    if (hasTarget_) requestRebuild();
  }
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
  expanders_.clear();
  discriminators_.clear();
  built_ = 0;
  bodyDepth_ = 0;
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
  // A list in another file of this project may be the id source of a combo here; ids of THIS file ("@units") are refreshed too.
  for (const auto& f : fields_)
    if (auto* ch = qobject_cast<ChoiceField*>(f.data()))
      if (!ch->node()->refs.empty() && !ch->node()->refs.front().empty() && ch->node()->refs.front()[0] == '@') ch->refreshItems();
  if (!pathStartsWith(c.path, target_) && !pathStartsWith(target_, c.path)) return;
  if (c.path.size() >= target_.size()) {
    // Inside the shown object: a change to one leaf's value refreshes just that field (keeps focus and typing position).
    JsonPath leaf = c.path;
    if (c.kind != JsonChange::Kind::Set && !c.key.empty()) leaf.push_back(PathStep::member(c.key));
    // Changing a key that decides which fields or shape apply (kind, a variant's tag key) rebuilds instead.
    if (discriminators_.contains(pathToString(leaf))) {
      requestRebuild();
      return;
    }
    if (c.kind == JsonChange::Kind::Set || !c.key.empty())
      if (FieldBase* f = fieldAt(leaf)) {
        // A key that only applies for some kind appears or disappears with its presence: rebuild then.
        if (f->node()->when && c.kind != JsonChange::Kind::Set) {
          requestRebuild();
          return;
        }
        f->refresh();
        return;
      }
  }
  requestRebuild();
}

// --- lazy containers --------------------------------------------------------------------------------------------

bool FormView::expandAlong(const JsonPath& path) {
  bool any = false;
  for (size_t n = target_.size() + 1; n <= path.size(); ++n) {
    const QString key = pathToString(JsonPath(path.begin(), path.begin() + (std::ptrdiff_t)n));
    auto it = expanders_.find(key);
    if (it == expanders_.end()) continue;
    const std::function<void()> open = it.value();
    open();  // removes itself from expanders_
    any = true;
  }
  return any;
}

void FormView::expandAll() {
  // Opening a container builds its children, which may contain collapsed ones: repeat until nothing is left (bounded).
  for (int pass = 0; pass < 30 && !expanders_.isEmpty(); ++pass) {
    const QList<std::function<void()>> all = expanders_.values();
    for (const auto& open : all) open();
  }
}

bool FormView::focusPath(const JsonPath& path) {
  flush();
  expandAlong(path);
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
  auto find = [&]() -> FieldBase* {
    FieldBase* best = nullptr;
    size_t bestDepth = ~size_t(0);
    for (const auto& f : fields_) {
      if (!f || f->path().empty() || f->path().back().isIndex || f->path().back().key != k) continue;
      if (f->path().size() < bestDepth) { best = f; bestDepth = f->path().size(); }
    }
    return best;
  };
  FieldBase* best = find();
  if (!best && !expanders_.isEmpty()) {  // maybe inside a collapsed block: open them all and look again
    expandAll();
    best = find();
  }
  if (best) return focusPath(best->path());
  for (auto it = anchors_.cbegin(); it != anchors_.cend(); ++it) {
    if (!it.value()) continue;
    const QString suffix = QStringLiteral(".") + key;
    if (it.key().endsWith(suffix) || it.key() == key) { ensureWidgetVisible(it.value()); return true; }
  }
  return false;
}

}  // namespace ote_native
