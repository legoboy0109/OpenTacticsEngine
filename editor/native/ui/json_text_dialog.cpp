#include "json_text_dialog.h"

#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace ote_native {

JsonTextDialog::JsonTextDialog(const QString& title, const ote::Json& initial, QWidget* parent) : QDialog(parent) {
  setObjectName("jsonTextDialog");
  setWindowTitle(title);
  edit_ = new QPlainTextEdit(this);
  edit_->setObjectName("jsonTextEdit");
  edit_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  edit_->setLineWrapMode(QPlainTextEdit::NoWrap);
  edit_->setTabChangesFocus(true);
  edit_->setPlainText(QString::fromStdString(initial.dump(2)));

  message_ = new QLabel(this);
  message_->setObjectName("jsonTextMessage");
  message_->setWordWrap(true);
  message_->setMinimumHeight(message_->fontMetrics().height() * 2);

  buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons_, &QDialogButtonBox::accepted, this, &JsonTextDialog::accept);
  connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(edit_, 1);
  layout->addWidget(message_);
  layout->addWidget(buttons_);
  resize(520, 420);

  connect(edit_, &QPlainTextEdit::textChanged, this, &JsonTextDialog::revalidate);
  revalidate();
}

QString JsonTextDialog::text() const { return edit_->toPlainText(); }
void JsonTextDialog::setText(const QString& text) { edit_->setPlainText(text); }
const QString& JsonTextDialog::errorText() const { return error_; }
QPushButton* JsonTextDialog::okButton() const { return buttons_->button(QDialogButtonBox::Ok); }

void JsonTextDialog::revalidate() {
  ote::Json parsed;
  std::string err;
  valid_ = ote::Json::parse(edit_->toPlainText().toStdString(), parsed, err);
  if (valid_) {
    value_ = std::move(parsed);
    error_.clear();
    message_->setText(tr("Valid JSON."));
    message_->setStyleSheet(QString());  // palette colours: readable on a light or a dark theme
  } else {
    error_ = tr("Not valid JSON (%1). Fix it to keep this value.").arg(QString::fromStdString(err));
    message_->setText(error_);
    // A fixed red that reads on both light and dark backgrounds; the text itself is the message, colour only helps.
    message_->setStyleSheet("color: #e5534b; font-weight: bold;");
  }
  okButton()->setEnabled(valid_);
}

void JsonTextDialog::accept() {
  if (!valid_) return;
  QDialog::accept();
}

}  // namespace ote_native
