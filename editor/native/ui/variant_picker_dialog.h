// The "which kind of command / condition / ..." chooser behind a list's Add button: a filter box over a list of names with their
// one-line help. A real list (not a menu) because a command list has more than sixty kinds.
#pragma once
#include <QDialog>
#include <QString>
#include <utility>
#include <vector>

class QLineEdit;
class QListWidget;

namespace ote_native {

class VariantPickerDialog : public QDialog {
  Q_OBJECT
 public:
  // entries: (name, help). Returns the index into `entries` of the chosen row, or -1 when cancelled.
  static int pick(const QString& title, const std::vector<std::pair<QString, QString>>& entries, QWidget* parent);

 private:
  VariantPickerDialog(const QString& title, const std::vector<std::pair<QString, QString>>& entries, QWidget* parent);
  void applyFilter(const QString& text);
  int chosen() const;

  QLineEdit* filter_;
  QListWidget* list_;
};

}  // namespace ote_native
