#include "form_parts.h"

#include <QFontMetrics>
#include <QPainter>
#include <QToolButton>

namespace ote_native {

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

ElidedLabel::ElidedLabel(const QString& text, QWidget* parent) : QLabel(text, parent) { setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); }

QSize ElidedLabel::minimumSizeHint() const { return QSize(24, QLabel::minimumSizeHint().height()); }

void ElidedLabel::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setPen(palette().color(foregroundRole()));
  p.setFont(font());
  p.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter, fontMetrics().elidedText(text(), Qt::ElideRight, width()));
}

QString summaryOf(const SchemaNode& itemNode, const ote::Json& item) {
  if (itemNode.kind == SchemaNode::Kind::Variant) {
    const int i = variantIndex(itemNode, item);
    if (i >= 0 && !itemNode.variants[(size_t)i].summary.empty())
      return summaryText(QString::fromStdString(itemNode.variants[(size_t)i].summary), item);
    QString name = i >= 0 ? QString::fromStdString(itemNode.variants[(size_t)i].name) : QString();
    if (item.isString()) name += QStringLiteral(" ") + QString::fromStdString(item.asString());
    return name;
  }
  if (!item.isObject()) return {};
  if (!itemNode.summary.empty()) return summaryText(QString::fromStdString(itemNode.summary), item);
  for (const char* k : {"id", "name", "at", "text", "job", "skill", "ability", "when"}) {
    const ote::Json& v = item[k];
    if (v.isString() && !v.asString().empty()) return QString::fromStdString(v.asString());
    if (v.isNumber()) return QStringLiteral("%1 %2").arg(k).arg(v.asNumber());
  }
  // Story commands, conditions and the like: the first key says what the item is.
  if (!item.members().empty()) return QString::fromStdString(item.members().front().first);
  return {};
}

JsonPath childPath(const JsonPath& p, const std::string& key) {
  JsonPath c = p;
  c.push_back(PathStep::member(key));
  return c;
}
JsonPath childPath(const JsonPath& p, size_t index) {
  JsonPath c = p;
  c.push_back(PathStep::at(index));
  return c;
}

bool isCompound(const SchemaNode& n) {
  switch (n.kind) {
    case SchemaNode::Kind::List: return n.length < 0;
    case SchemaNode::Kind::Object: case SchemaNode::Kind::Map: case SchemaNode::Kind::Variant: return true;
    default: return false;
  }
}

}  // namespace ote_native
