#include "prompter.h"

#include <QMessageBox>
#include <QPushButton>

namespace ote_native {

bool MessageBoxPrompter::confirmCommentLoss(const QString& relPath) {
  QMessageBox box(QMessageBox::Warning, QObject::tr("Comments will be removed"),
                  QObject::tr("%1 contains // comments. The editor cannot keep comments, so saving will remove them from this file.\n\n"
                              "(Everything else in the file is kept as it is.)").arg(relPath),
                  QMessageBox::NoButton, parent_);
  QPushButton* save = box.addButton(QObject::tr("Save anyway"), QMessageBox::AcceptRole);
  box.addButton(QMessageBox::Cancel);
  box.exec();
  return box.clickedButton() == save;
}

Prompter::ConflictChoice MessageBoxPrompter::resolveConflict(const QString& relPath, const QString& message) {
  QMessageBox box(QMessageBox::Warning, QObject::tr("File changed on disk"),
                  QObject::tr("%1\n\nSomething else changed this file after the editor loaded it. "
                              "Reload shows what is on disk now and drops your unsaved changes to it; "
                              "Keep mine saves your version over it.").arg(message),
                  QMessageBox::NoButton, parent_);
  box.setInformativeText(relPath);
  QPushButton* reload = box.addButton(QObject::tr("Reload from disk"), QMessageBox::DestructiveRole);
  QPushButton* mine = box.addButton(QObject::tr("Keep mine"), QMessageBox::AcceptRole);
  box.addButton(QMessageBox::Cancel);
  box.exec();
  if (box.clickedButton() == reload) return ConflictChoice::Reload;
  if (box.clickedButton() == mine) return ConflictChoice::KeepMine;
  return ConflictChoice::Cancel;
}

Prompter::SaveChoice MessageBoxPrompter::askSaveChanges(const QStringList& dirtyFiles, const QString& why) {
  QMessageBox box(QMessageBox::Question, QObject::tr("Unsaved changes"),
                  QObject::tr("%1\n\nUnsaved: %2").arg(why, dirtyFiles.join(", ")), QMessageBox::NoButton, parent_);
  QPushButton* save = box.addButton(QObject::tr("Save all"), QMessageBox::AcceptRole);
  QPushButton* discard = box.addButton(QObject::tr("Don't save"), QMessageBox::DestructiveRole);
  box.addButton(QMessageBox::Cancel);
  box.exec();
  if (box.clickedButton() == save) return SaveChoice::Save;
  if (box.clickedButton() == discard) return SaveChoice::Discard;
  return SaveChoice::Cancel;
}

void MessageBoxPrompter::showError(const QString& title, const QString& message) { QMessageBox::critical(parent_, title, message); }

}  // namespace ote_native
