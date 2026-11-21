#include "map_resize_dialog.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <map>

#include "map_painter.h"

namespace ote_native {

MapResizeDialog::MapResizeDialog(MapPainter* painter, QWidget* parent) : QDialog(parent), painter_(painter) {
  setObjectName("mapResizeDialog");
  setWindowTitle(tr("Resize map"));
  const ote_editor::MapGrid& g = painter->grid();
  width_ = new QSpinBox(this);
  width_->setRange(1, 256);
  width_->setValue(g.width);
  height_ = new QSpinBox(this);
  height_->setRange(1, 256);
  height_->setValue(g.height);
  width_->setObjectName("resizeWidth");
  height_->setObjectName("resizeHeight");

  // The anchor: which part of the old map stays where it is (the middle square = centred).
  auto* grid = new QGridLayout;
  anchors_ = new QButtonGroup(this);
  const char* arrows[9] = {"↖", "↑", "↗", "←", "●", "→", "↙", "↓", "↘"};
  for (int i = 0; i < 9; ++i) {
    auto* b = new QToolButton(this);
    b->setText(QString::fromUtf8(arrows[i]));
    b->setCheckable(true);
    b->setFixedSize(34, 30);
    b->setToolTip(tr("The old map stays at this side or corner"));
    anchors_->addButton(b, i);
    grid->addWidget(b, i / 3, i % 3);
  }
  anchors_->setExclusive(true);
  anchors_->button(0)->setChecked(true);
  auto* anchorBox = new QWidget(this);
  anchorBox->setLayout(grid);

  fill_ = new QComboBox(this);
  fill_->setObjectName("resizeFill");
  std::map<std::string, int> count;  // default: the terrain the map uses most
  for (const std::string& r : g.rows)
    for (char c : r) ++count[g.terrainOf(c)];
  int best = -1, bestCount = -1;
  for (const auto& e : g.legend) {
    fill_->addItem(QString::fromStdString(e.second), QString::fromStdString(std::string(1, e.first)));
    if (count[e.second] > bestCount) {
      bestCount = count[e.second];
      best = fill_->count() - 1;
    }
  }
  fill_->setCurrentIndex(best);

  warning_ = new QLabel(this);
  warning_->setObjectName("resizeWarning");
  warning_->setWordWrap(true);
  warning_->setStyleSheet("color: #e5534b;");

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto* form = new QFormLayout;
  form->addRow(tr("Width (squares)"), width_);
  form->addRow(tr("Height (squares)"), height_);
  form->addRow(tr("Keep the old map at"), anchorBox);
  form->addRow(tr("Fill new squares with"), fill_);
  auto* root = new QVBoxLayout(this);
  root->addLayout(form);
  root->addWidget(new QLabel(tr("Units, structures and deployment slots move with the old map. Story events keep their own coordinates."), this));
  root->addWidget(warning_);
  root->addWidget(buttons);

  for (QSpinBox* s : {width_, height_}) connect(s, qOverload<int>(&QSpinBox::valueChanged), this, [this] { refreshWarning(); });
  connect(anchors_, &QButtonGroup::idClicked, this, [this] { refreshWarning(); });
  refreshWarning();
}

ote_editor::ResizeSpec MapResizeDialog::spec() const {
  ote_editor::ResizeSpec s;
  s.width = width_->value();
  s.height = height_->value();
  const int a = anchors_->checkedId() < 0 ? 0 : anchors_->checkedId();
  s.anchorX = a % 3;
  s.anchorY = a / 3;
  const QString sym = fill_->currentData().toString();
  s.fill = sym.isEmpty() ? '.' : sym.at(0).toLatin1();
  return s;
}

QString MapResizeDialog::describeLost(const std::vector<ote_editor::LostThing>& lost) {
  if (lost.empty()) return QString();
  QStringList parts;
  for (size_t i = 0; i < lost.size() && i < 8; ++i)
    parts << QObject::tr("%1 at %2, %3").arg(QString::fromStdString(lost[i].kind + (lost[i].label.empty() ? "" : " " + lost[i].label))).arg(lost[i].x).arg(lost[i].y);
  QString text = QObject::tr("%1 thing(s) no longer fit and will be removed: ").arg(lost.size()) + parts.join(QStringLiteral("; "));
  if (lost.size() > 8) text += QObject::tr("; and %1 more").arg(lost.size() - 8);
  return text;
}

void MapResizeDialog::refreshWarning() {
  QString err;
  warning_->setText(describeLost(painter_->lostByResize(spec(), &err)));
}

}  // namespace ote_native
