// The project's data files as a folder tree. Each file shows whether it has unsaved edits, whether it changed on disk
// behind the editor's back, and how many problems the last check found in it.
#pragma once
#include <QDockWidget>
#include <QHash>

class QTreeWidget;
class QTreeWidgetItem;

namespace ote_native {

class FileListDock : public QDockWidget {
  Q_OBJECT
 public:
  explicit FileListDock(QWidget* parent = nullptr);

  void setFiles(const QStringList& relPaths);
  void setFileState(const QString& rel, bool dirty, bool changedOnDisk);
  void setProblemCounts(const QHash<QString, int>& countsByFile);
  void selectFile(const QString& rel);
  QString textOf(const QString& rel) const;  // what the user sees for that file
  QTreeWidget* tree() const { return tree_; }

 signals:
  void openFileRequested(const QString& relPath);

 private:
  void refreshText(const QString& rel);

  QTreeWidget* tree_;
  QHash<QString, QTreeWidgetItem*> items_;
  QHash<QString, bool> dirty_, external_;
  QHash<QString, int> problems_;
};

}  // namespace ote_native
