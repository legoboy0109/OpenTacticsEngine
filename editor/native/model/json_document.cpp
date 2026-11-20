#include "json_document.h"

#include <QCryptographicHash>
#include <QFile>
#include <algorithm>
#include <climits>
#include <utility>

#include "comment_scan.h"

namespace ote_native {

JsonDocument::JsonDocument(QString relPath, QString absPath) : relPath_(std::move(relPath)), absPath_(std::move(absPath)) {}

QByteArray JsonDocument::hashOf(const QByteArray& bytes) { return QCryptographicHash::hash(bytes, QCryptographicHash::Sha1); }

bool JsonDocument::load(QString* error) {
  QFile f(absPath_);
  if (!f.open(QIODevice::ReadOnly)) {
    if (error) *error = QStringLiteral("%1: cannot read the file (%2)").arg(relPath_, f.errorString());
    return false;
  }
  QByteArray bytes = f.readAll();
  f.close();
  const std::string text = bytes.toStdString();
  ote::Json parsed;
  std::string err;
  if (!ote::Json::parse(text, parsed, err)) {
    if (error) *error = QStringLiteral("%1: %2").arg(relPath_, QString::fromStdString(err));
    return false;
  }
  root_ = std::move(parsed);
  hasComments_ = jsonTextHasComments(text);
  commentsAcknowledged_ = false;
  externallyChanged_ = false;
  version_ = savedVersion_ = 0;
  ++generation_;
  diskHash_ = hashOf(bytes);
  emit reloaded();
  emit stateChanged();
  return true;
}

QByteArray JsonDocument::serialize() const {
  std::string text = root_.dump(2);
  text += '\n';
  return QByteArray::fromStdString(text);
}

void JsonDocument::bumpVersion(int delta) {
  const bool was = isDirty();
  version_ += delta;
  if (was != isDirty()) emit stateChanged();
}

void JsonDocument::forgetUnreachableSavedState() {
  if (savedVersion_ > version_) {
    const bool was = isDirty();
    savedVersion_ = INT_MIN;  // never equals a real version again
    if (was != isDirty()) emit stateChanged();
  }
}

void JsonDocument::markSaved(const QByteArray& diskBytes) {
  const bool wasDirty = isDirty(), wasExt = externallyChanged_;
  savedVersion_ = version_;
  diskHash_ = hashOf(diskBytes);
  externallyChanged_ = false;
  if (wasDirty != isDirty() || wasExt != externallyChanged_) emit stateChanged();
}

void JsonDocument::setExternallyChanged(bool v) {
  if (externallyChanged_ == v) return;
  externallyChanged_ = v;
  emit stateChanged();
}

bool JsonDocument::applySet(const JsonPath& path, const ote::Json& value) {
  ote::Json* node = resolvePath(root_, path);
  if (!node) return false;
  JsonChange c;
  c.kind = JsonChange::Kind::Set;
  c.path = path;
  emit aboutToChange(c);
  *node = value;
  emit changed(c);
  return true;
}

bool JsonDocument::applyInsert(const JsonPath& container, size_t index, const std::string& key, const ote::Json& value) {
  ote::Json* node = resolvePath(root_, container);
  if (!node) return false;
  if (node->isArray()) {
    if (index > node->items().size()) return false;
  } else if (node->isObject()) {
    if (index > node->members().size()) return false;
    for (const auto& m : node->members())
      if (m.first == key) return false;  // keys stay unique
  } else {
    return false;
  }
  JsonChange c;
  c.kind = JsonChange::Kind::Insert;
  c.path = container;
  c.index = index;
  c.key = key;
  emit aboutToChange(c);
  if (node->isArray()) node->mutableItems().insert(node->mutableItems().begin() + (std::ptrdiff_t)index, value);
  else node->mutableMembers().insert(node->mutableMembers().begin() + (std::ptrdiff_t)index, ote::Json::Member(key, value));
  emit changed(c);
  return true;
}

bool JsonDocument::applyErase(const JsonPath& container, size_t index) {
  ote::Json* node = resolvePath(root_, container);
  if (!node) return false;
  JsonChange c;
  c.kind = JsonChange::Kind::Erase;
  c.path = container;
  c.index = index;
  if (node->isArray()) {
    if (index >= node->items().size()) return false;
  } else if (node->isObject()) {
    if (index >= node->members().size()) return false;
    c.key = node->members()[index].first;
  } else {
    return false;
  }
  emit aboutToChange(c);
  if (node->isArray()) node->mutableItems().erase(node->mutableItems().begin() + (std::ptrdiff_t)index);
  else node->mutableMembers().erase(node->mutableMembers().begin() + (std::ptrdiff_t)index);
  emit changed(c);
  return true;
}

bool JsonDocument::applyMove(const JsonPath& container, size_t from, size_t to) {
  ote::Json* node = resolvePath(root_, container);
  if (!node || from == to) return false;
  const size_t n = node->isArray() ? node->items().size() : node->isObject() ? node->members().size() : 0;
  if (from >= n || to >= n) return false;
  JsonChange c;
  c.kind = JsonChange::Kind::Move;
  c.path = container;
  c.index = from;
  c.to = to;
  emit aboutToChange(c);
  // Rotate so that the element at `from` ends up at `to` and the ones in between shift by one.
  auto rotate = [&](auto& v) {
    if (from < to) std::rotate(v.begin() + (std::ptrdiff_t)from, v.begin() + (std::ptrdiff_t)from + 1, v.begin() + (std::ptrdiff_t)to + 1);
    else std::rotate(v.begin() + (std::ptrdiff_t)to, v.begin() + (std::ptrdiff_t)from, v.begin() + (std::ptrdiff_t)from + 1);
  };
  if (node->isArray()) rotate(node->mutableItems());
  else rotate(node->mutableMembers());
  emit changed(c);
  return true;
}

}  // namespace ote_native
