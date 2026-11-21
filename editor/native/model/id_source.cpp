#include "id_source.h"

#include "json_document.h"
#include "project.h"
#include "table_shape.h"

namespace ote_native {

IdSource::IdSource(Project* project, QObject* parent) : QObject(parent), project_(project) {}

QString IdSource::fileOf(const std::string& source) {
  QString s = QString::fromStdString(source);
  if (s.startsWith("tags:")) s = s.mid(5);
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
  QStringList out;
  for (const std::string& src : sources) {
    const QString rel = fileOf(src);
    if (!project_->jsonFiles().contains(rel)) continue;
    JsonDocument* doc = project_->document(rel);
    if (!doc) continue;
    watch(rel);
    const ote::Json* list = &doc->root();
    const QString s = QString::fromStdString(src);
    if (s.startsWith("tags:")) {
      // "tags:data/units.json": the free words in the rows' "tags" lists (what unit filters and conditions may name).
      if (auto p = findTableArray(doc->root()))
        for (const ote::Json& row : resolvePath(doc->root(), *p)->items())
          for (const ote::Json& t : row["tags"].items())
            if (t.isString() && !t.asString().empty() && !out.contains(QString::fromStdString(t.asString()))) out.append(QString::fromStdString(t.asString()));
      continue;
    }
    const int hash = s.indexOf('#');
    if (hash >= 0) {
      for (const QString& part : s.mid(hash + 1).split('.')) {
        list = &(*list)[part.toStdString()];
      }
    } else if (auto p = findTableArray(doc->root())) {
      // An empty table array is not recognised by findTableArray (it says nothing about columns); fall through to the key.
      list = resolvePath(doc->root(), *p);
    } else if (doc->root().isObject() && doc->root().members().size() == 1) {
      list = &doc->root().members()[0].second;
    }
    for (const ote::Json& item : list->items()) {
      const QString id = QString::fromStdString(item.isString() ? item.asString() : item["id"].asString());
      if (!id.isEmpty() && !out.contains(id)) out.append(id);
    }
  }
  return out;
}

}  // namespace ote_native
