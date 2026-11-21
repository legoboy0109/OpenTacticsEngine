// The "Other keys" box of a form: what is in the file but not described by the schema, kept exactly as it is.
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>

#include "form_parts.h"
#include "form_view.h"
#include "project.h"

namespace ote_native {

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
      if (!result.has(m.first.c_str())) ctx_->erase(childPath(path, m.first));
    for (const auto& m : result.members()) ctx_->set(childPath(path, m.first), m.second);
    stack->endMacro();
    (void)n;
  });
  anchors_.insert(pathToString(path) + "#other", box);
  return box;
}

}  // namespace ote_native
