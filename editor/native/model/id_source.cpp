#include "id_source.h"

#include "json_document.h"
#include "project.h"
#include "schema_ids.h"

namespace ote_native {

IdSource::IdSource(Project* project, QObject* parent) : QObject(parent), project_(project) {}

QString IdSource::fileOf(const std::string& source) {
  QString s = QString::fromStdString(source);
  if (s.startsWith("tags:")) s = s.mid(5);
  else if (s.startsWith("keys:")) s = s.mid(5);
  else if (s.startsWith("field:")) s = s.mid(s.indexOf(':', 6) + 1);
  const int hash = s.indexOf('#');
  return hash < 0 ? s : s.left(hash);
}

void IdSource::watch(const QString& rel) {
  if (watched_.contains(rel)) return;
  JsonDocument* d = project_->loadedDocument(rel);
  if (!d) return;
  watched_.insert(rel);
  // Only signals that can change an id list; the form side-steps its own rebuild by listening to the same document.
  connect(d, &JsonDocument::changed, this, &IdSource::idsChanged);
  connect(d, &JsonDocument::reloaded, this, &IdSource::idsChanged);
}

QStringList IdSource::ids(const std::vector<std::string>& sources) {
  // The resolving itself is ote_editor::resolveIdSources (shared with ote-mcp); this only feeds it the project's open
  // documents and remembers which ones to watch. The edited document is not needed here ("@..." sources are the form's).
  const ote::Json none;
  QStringList out;
  for (const std::string& id : ote_editor::resolveIdSources(none, docLookup(), sources)) out << QString::fromStdString(id);
  return out;
}

ote_editor::DocLookup IdSource::docLookup() {
  return [this](const std::string& relStd) -> const ote::Json* {
    const QString rel = QString::fromStdString(relStd);
    if (!project_->jsonFiles().contains(rel)) return nullptr;
    JsonDocument* doc = project_->document(rel);
    if (!doc) return nullptr;
    watch(rel);
    return &doc->root();
  };
}

}  // namespace ote_native
