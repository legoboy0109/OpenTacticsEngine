#include "project_session.h"

namespace ote_native {

ProjectSession::ProjectSession(const QString& folder, Prompter* prompter, QObject* parent)
    : QObject(parent), project_(folder), prompter_(prompter) {
  // Validation runs on the saved files, so it follows every successful save.
  connect(&project_, &Project::saved, this, [this] { checkProblems(); });
}

void ProjectSession::checkProblems() { problems_.run(project_.folder()); }

bool ProjectSession::save(const QString& rel) {
  JsonDocument* doc = project_.loadedDocument(rel);
  if (!doc || !doc->isDirty()) return true;
  // Saving drops // comments (ote::Json does not keep them): say so once, before the first save of such a file.
  if (doc->hasComments() && !doc->commentsAcknowledged()) {
    if (!prompter_->confirmCommentLoss(rel)) return false;
    doc->setCommentsAcknowledged(true);
  }
  SaveResult r = project_.save(rel);
  if (r.status == SaveResult::Status::Conflict) {
    switch (prompter_->resolveConflict(rel, r.message)) {
      case Prompter::ConflictChoice::Cancel: return false;
      case Prompter::ConflictChoice::Reload: {
        QString err;
        if (!project_.reload(rel, &err)) { prompter_->showError(tr("Reload failed"), err); return false; }
        emit statusMessage(tr("Reloaded %1 from disk; your unsaved changes to it were dropped.").arg(rel));
        return false;  // nothing was saved
      }
      case Prompter::ConflictChoice::KeepMine: r = project_.save(rel, /*overwrite=*/true); break;
    }
  }
  if (r.status == SaveResult::Status::Failed) {
    prompter_->showError(tr("Save failed"), r.message);
    return false;
  }
  emit statusMessage(tr("Saved %1").arg(rel));
  return true;
}

bool ProjectSession::saveAll() {
  bool all = true;
  for (const QString& rel : project_.dirtyFiles()) all = save(rel) && all;
  return all;
}

bool ProjectSession::confirmLeave(const QString& why) {
  const QStringList dirty = project_.dirtyFiles();
  if (dirty.isEmpty()) return true;
  switch (prompter_->askSaveChanges(dirty, why)) {
    case Prompter::SaveChoice::Save: return saveAll();
    case Prompter::SaveChoice::Discard: return true;
    default: return false;
  }
}

}  // namespace ote_native
