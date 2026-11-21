#include "story_ids.h"

#include "id_source.h"

namespace ote_native {

namespace {
QStringList toQt(const std::vector<std::string>& l) {
  QStringList out;
  for (const std::string& s : l) out << QString::fromStdString(s);
  return out;
}
}  // namespace

// The computation is in editor/core (schema_ids.cpp), shared with ote-mcp.
QStringList storyUnitIds(const ote::Json& map) { return toQt(ote_editor::storyUnitIds(map)); }
QStringList teamIds(const ote::Json& map) { return toQt(ote_editor::teamIds(map)); }

QStringList resolveSources(const ote::Json& doc, IdSource* ids, const std::vector<std::string>& sources) {
  return toQt(ote_editor::resolveIdSources(doc, ids ? ids->docLookup() : ote_editor::DocLookup(), sources));
}

}  // namespace ote_native
