#include "variant_picker_dialog.h"

#include <QDialogButtonBox>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

namespace ote_native {

VariantPickerDialog::VariantPickerDialog(const QString& title, const std::vector<std::pair<QString, QString>>& entries, QWidget* parent)
    : QDialog(parent) {
  setWindowTitle(title);
  setObjectName("variantPicker");
  resize(560, 460);
  auto* v = new QVBoxLayout(this);
  filter_ = new QLineEdit(this);
  filter_->setObjectName("variantPickerFilter");
  filter_->setPlaceholderText(tr("type to filter"));
  filter_->setClearButtonEnabled(true);
  v->addWidget(filter_);
  list_ = new QListWidget(this);
  list_->setObjectName("variantPickerList");
  for (size_t i = 0; i < entries.size(); ++i) {
    // One row: the raw key in front, what it does after it. The row's data is the index into the caller's list.
    auto* item = new QListWidgetItem(entries[i].second.isEmpty() ? entries[i].first : entries[i].first + QStringLiteral("  —  ") + entries[i].second, list_);
    item->setData(Qt::UserRole, (int)i);
    item->setToolTip(entries[i].second);
  }
  v->addWidget(list_, 1);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  v->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(list_, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
  connect(filter_, &QLineEdit::textChanged, this, &VariantPickerDialog::applyFilter);
  if (list_->count() > 0) list_->setCurrentRow(0);
  filter_->setFocus();
}

void VariantPickerDialog::applyFilter(const QString& text) {
  int firstShown = -1;
  for (int r = 0; r < list_->count(); ++r) {
    const bool show = text.isEmpty() || list_->item(r)->text().contains(text, Qt::CaseInsensitive);
    list_->item(r)->setHidden(!show);
    if (show && firstShown < 0) firstShown = r;
  }
  if (firstShown >= 0) list_->setCurrentRow(firstShown);
}

int VariantPickerDialog::chosen() const {
  const QListWidgetItem* it = list_->currentItem();
  return it && !it->isHidden() ? it->data(Qt::UserRole).toInt() : -1;
}

int VariantPickerDialog::pick(const QString& title, const std::vector<std::pair<QString, QString>>& entries, QWidget* parent) {
  VariantPickerDialog dlg(title, entries, parent);
  return dlg.exec() == QDialog::Accepted ? dlg.chosen() : -1;
}

}  // namespace ote_native
