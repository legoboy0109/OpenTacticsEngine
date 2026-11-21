// Answers "which ids does this reference point at?" from the project's live documents, so an id combo box always shows
// what the referenced file holds right now (unsaved edits included) and is told when that changes.
// A source is "data/units.json" (the ids of its table rows) or "game.json#damageTypes" (the list under that key path in
// that file; items are objects with an "id", or plain strings).
#pragma once
#include <QObject>
#include <QSet>
#include <QStringList>
#include <vector>

#include "schema_ids.h"

namespace ote_native {

class Project;

class IdSource : public QObject {
  Q_OBJECT
 public:
  explicit IdSource(Project* project, QObject* parent = nullptr);

  // Ids of every source, in order, without duplicates. A missing or unreadable file contributes nothing.
  QStringList ids(const std::vector<std::string>& sources);
  static QString fileOf(const std::string& source);
  // The project's open documents as the core resolver wants them (also watches what it reads).
  ote_editor::DocLookup docLookup();

 signals:
  // Some referenced document changed (an edit, undo, reload). Listeners refill their combo boxes.
  void idsChanged();

 private:
  void watch(const QString& rel);

  Project* project_;
  QSet<QString> watched_;
};

}  // namespace ote_native
