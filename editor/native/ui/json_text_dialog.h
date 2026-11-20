// A small dialog to edit one JSON value as text (the nested objects and arrays of a table cell or tree node).
// It refuses to close with OK while the text is not valid JSON, and says why in plain words next to the text.
#pragma once
#include <QDialog>

#include "core/base/json.h"

class QDialogButtonBox;
class QLabel;
class QPlainTextEdit;

namespace ote_native {

class JsonTextDialog : public QDialog {
  Q_OBJECT
 public:
  JsonTextDialog(const QString& title, const ote::Json& initial, QWidget* parent = nullptr);

  QString text() const;
  void setText(const QString& text);
  // The edited value; only meaningful when isValid().
  const ote::Json& value() const { return value_; }
  bool isValid() const { return valid_; }
  const QString& errorText() const;
  QPushButton* okButton() const;
  QLabel* messageLabel() const { return message_; }

  void accept() override;  // refuses while invalid

 private:
  void revalidate();

  QPlainTextEdit* edit_;
  QLabel* message_;
  QDialogButtonBox* buttons_;
  ote::Json value_;
  bool valid_ = true;
  QString error_;
};

}  // namespace ote_native
