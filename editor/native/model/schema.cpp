#include "schema.h"

#include <QDir>
#include <QFile>

namespace ote_native {

namespace {
// The core engine's id lookup speaks std::string; the forms' speaks QStringList.
ote_editor::IdLookup toCore(const IdLookup& ids) {
  if (!ids) return nullptr;
  return [ids](const std::vector<std::string>& sources) {
    std::vector<std::string> out;
    for (const QString& s : ids(sources)) out.push_back(s.toStdString());
    return out;
  };
}
}  // namespace

void SchemaSet::loadFolder(const QString& dir) {
  if (!dir.startsWith(QLatin1Char(':'))) {  // a real folder: the core reads it
    core_.loadFolder(dir.toStdString());
    return;
  }
  // A Qt resource folder: gather the files here (two passes inside addTexts: every file's defs before any file's nodes).
  const QDir d(dir);
  std::vector<ote_editor::SchemaSet::Text> texts;
  for (const QString& n : d.entryList({"*.json"}, QDir::Files, QDir::Name)) {
    QFile f(d.filePath(n));
    if (!f.open(QIODevice::ReadOnly)) {
      readErrors_ << n + ": cannot be read";
      continue;
    }
    texts.emplace_back(n.toStdString(), f.readAll().toStdString());
  }
  core_.addTexts(texts);
}

void SchemaSet::addFromText(const QString& name, const QByteArray& text) { core_.addFromText(name.toStdString(), text.toStdString()); }

QStringList SchemaSet::errors() const {
  QStringList out = readErrors_;
  for (const std::string& e : core_.errors()) out << QString::fromStdString(e);
  return out;
}

SchemaSet SchemaSet::loadDefault(const QString& overrideDir, const QString& appDir) {
  SchemaSet set;
  if (!overrideDir.isEmpty() && QDir(overrideDir).exists()) set.loadFolder(overrideDir);
  else if (QDir(appDir + "/schema").exists()) set.loadFolder(appDir + "/schema");
  else set.loadFolder(QStringLiteral(":/schema"));
  return set;
}

bool nodeAccepts(const SchemaNode& n, const ote::Json& v, QString* why) {
  std::string w;
  const bool ok = ote_editor::schemaAccepts(n, v, why ? &w : nullptr);
  if (!ok && why) *why = QString::fromStdString(w);
  return ok;
}

ote::Json defaultValue(const SchemaNode& n, const IdLookup& ids) { return ote_editor::schemaDefault(n, toCore(ids)); }
QString summaryText(const QString& tmpl, const ote::Json& value) { return QString::fromStdString(ote_editor::schemaSummary(tmpl.toStdString(), value)); }
ote::Json defaultValueOfVariant(const SchemaNode::Variant& v, const IdLookup& ids) { return ote_editor::schemaVariantDefault(v, toCore(ids)); }
ote::Json sampleValueOfVariant(const SchemaNode::Variant& v, const IdLookup& ids) { return ote_editor::schemaVariantSample(v, toCore(ids)); }
ote::Json sampleValue(const SchemaNode& n, const IdLookup& ids) { return ote_editor::schemaSample(n, toCore(ids)); }

}  // namespace ote_native
