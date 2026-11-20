#include "json_tree_model.h"

#include <QFont>
#include <algorithm>

#include "cell_text.h"
#include "json_commands.h"
#include "project.h"

namespace ote_native {

JsonTreeModel::JsonTreeModel(Project* project, JsonDocument* doc, QObject* parent)
    : QAbstractItemModel(parent), project_(project), doc_(doc), root_(std::make_unique<Node>()) {
  connect(doc_, &JsonDocument::aboutToChange, this, &JsonTreeModel::onAboutToChange);
  connect(doc_, &JsonDocument::changed, this, &JsonTreeModel::onChanged);
  connect(doc_, &JsonDocument::reloaded, this, [this] {
    beginResetModel();
    root_ = std::make_unique<Node>();
    endResetModel();
  });
}

JsonTreeModel::~JsonTreeModel() = default;

QString JsonTreeModel::typeName(const ote::Json& j) {
  switch (j.type()) {
    case ote::Json::Type::Null: return QStringLiteral("null");
    case ote::Json::Type::Bool: return QStringLiteral("bool");
    case ote::Json::Type::Number: return QStringLiteral("number");
    case ote::Json::Type::String: return QStringLiteral("string");
    case ote::Json::Type::Array: return QStringLiteral("array");
    default: return QStringLiteral("object");
  }
}

JsonPath JsonTreeModel::pathOfNode(const Node* n) const {
  JsonPath p;
  for (; n && n->parent; n = n->parent) p.push_back(n->inArray ? PathStep::at((size_t)n->row) : PathStep::member(n->key));
  std::reverse(p.begin(), p.end());
  return p;
}

JsonPath JsonTreeModel::pathOf(const QModelIndex& index) const { return pathOfNode(nodeFor(index)); }

const ote::Json* JsonTreeModel::jsonAt(const QModelIndex& index) const { return resolvePath(doc_->root(), pathOf(index)); }

QModelIndex JsonTreeModel::indexOf(Node* n, int column) const {
  if (!n || n == root_.get()) return {};
  return createIndex(n->row, column, n);
}

std::unique_ptr<JsonTreeModel::Node> JsonTreeModel::makeNode(Node* parent, const ote::Json& parentJson, int row) const {
  auto k = std::make_unique<Node>();
  k->parent = parent;
  k->row = row;
  k->inArray = parentJson.isArray();
  if (parentJson.isObject()) k->key = parentJson.members()[(size_t)row].first;
  return k;
}

void JsonTreeModel::ensureKids(Node* n) const {
  if (n->built) return;
  n->built = true;
  const ote::Json* j = resolvePath(doc_->root(), pathOfNode(n));
  if (!j || !(j->isArray() || j->isObject())) return;
  const int count = (int)(j->isArray() ? j->items().size() : j->members().size());
  for (int i = 0; i < count; ++i) n->kids.push_back(makeNode(n, *j, i));
}

void JsonTreeModel::renumber(Node* n, int from) const {
  for (int i = std::max(0, from); i < (int)n->kids.size(); ++i) n->kids[(size_t)i]->row = i;
}

JsonTreeModel::Node* JsonTreeModel::findNode(const JsonPath& path) const {
  Node* n = root_.get();
  for (const PathStep& s : path) {
    if (!n->built) return nullptr;
    Node* next = nullptr;
    if (s.isIndex) {
      if (s.index < n->kids.size()) next = n->kids[s.index].get();
    } else {
      for (auto& k : n->kids)
        if (!k->inArray && k->key == s.key) { next = k.get(); break; }
    }
    if (!next) return nullptr;
    n = next;
  }
  return n;
}

QModelIndex JsonTreeModel::indexForPath(const JsonPath& path) const {
  Node* n = root_.get();
  for (const PathStep& s : path) {
    ensureKids(n);
    Node* next = nullptr;
    if (s.isIndex) {
      if (s.index < n->kids.size()) next = n->kids[s.index].get();
    } else {
      for (auto& k : n->kids)
        if (!k->inArray && k->key == s.key) { next = k.get(); break; }
    }
    if (!next) return {};
    n = next;
  }
  return indexOf(n);
}

QModelIndex JsonTreeModel::index(int row, int column, const QModelIndex& parent) const {
  if (column < 0 || column >= ColumnCount || row < 0) return {};
  Node* n = nodeFor(parent);
  ensureKids(n);
  if ((size_t)row >= n->kids.size()) return {};
  return createIndex(row, column, n->kids[(size_t)row].get());
}

QModelIndex JsonTreeModel::parent(const QModelIndex& child) const {
  if (!child.isValid()) return {};
  Node* p = nodeFor(child)->parent;
  if (!p || p == root_.get()) return {};
  return createIndex(p->row, 0, p);
}

int JsonTreeModel::rowCount(const QModelIndex& parent) const {
  if (parent.column() > 0) return 0;
  Node* n = nodeFor(parent);
  ensureKids(n);
  return (int)n->kids.size();
}

bool JsonTreeModel::hasChildren(const QModelIndex& parent) const {
  if (parent.column() > 0) return false;
  Node* n = nodeFor(parent);
  if (n->built) return !n->kids.empty();
  const ote::Json* j = resolvePath(doc_->root(), pathOfNode(n));
  return j && ((j->isArray() && j->size() > 0) || (j->isObject() && j->size() > 0));
}

QVariant JsonTreeModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
  switch (section) {
    case KeyColumn: return tr("Key");
    case ValueColumn: return tr("Value");
    case TypeColumn: return tr("Type");
    default: return {};
  }
}

QVariant JsonTreeModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid()) return {};
  Node* n = nodeFor(index);
  const ote::Json* j = jsonAt(index);
  if (!j) return {};
  const bool container = j->isArray() || j->isObject();
  switch (index.column()) {
    case KeyColumn:
      if (role == Qt::DisplayRole) return n->inArray ? QStringLiteral("[%1]").arg(n->row) : QString::fromStdString(n->key);
      if (role == Qt::EditRole) return QString::fromStdString(n->key);
      break;
    case ValueColumn:
      if (role == Qt::DisplayRole) {
        // Plain plural handling: no translation catalogue exists yet, so Qt's %n form would show "key(s)" literally.
        if (j->isObject()) return j->size() == 1 ? tr("{1 key}") : tr("{%1 keys}").arg(j->size());
        if (j->isArray()) return j->size() == 1 ? tr("[1 item]") : tr("[%1 items]").arg(j->size());
        return jsonToCellText(*j);
      }
      if (role == Qt::EditRole) return container ? QVariant() : QVariant(jsonToCellText(*j));
      if (role == Qt::CheckStateRole && j->isBool()) return j->asBool() ? Qt::Checked : Qt::Unchecked;
      if (role == Qt::FontRole && container) {
        QFont f;
        f.setItalic(true);
        return f;
      }
      break;
    case TypeColumn:
      if (role == Qt::DisplayRole) return typeName(*j);
      break;
  }
  return {};
}

Qt::ItemFlags JsonTreeModel::flags(const QModelIndex& index) const {
  Qt::ItemFlags f = QAbstractItemModel::flags(index);
  if (!index.isValid()) return f;
  Node* n = nodeFor(index);
  const ote::Json* j = jsonAt(index);
  if (index.column() == KeyColumn && !n->inArray) f |= Qt::ItemIsEditable;
  if (index.column() == ValueColumn && j && !(j->isArray() || j->isObject())) {
    f |= Qt::ItemIsEditable;
    if (j->isBool()) f |= Qt::ItemIsUserCheckable;
  }
  return f;
}

bool JsonTreeModel::setData(const QModelIndex& index, const QVariant& value, int role) {
  if (!index.isValid()) return false;
  if (index.column() == KeyColumn && role == Qt::EditRole) return renameKey(index, value.toString());
  if (index.column() != ValueColumn) return false;
  QString text;
  if (role == Qt::CheckStateRole) text = value.toInt() == Qt::Checked ? "true" : "false";
  else if (role == Qt::EditRole) text = value.toString();
  else return false;
  const ote::Json* cur = jsonAt(index);
  if (!cur) return false;
  CellParse p = parseCellText(text, cur, ote::Json::Type::Null);
  if (!p.ok) {
    emit editRejected(tr("%1: %2").arg(pathToString(pathOf(index)), p.error));
    return false;
  }
  return setValue(index, p.value);
}

bool JsonTreeModel::setValue(const QModelIndex& index, const ote::Json& value) {
  const ote::Json* cur = jsonAt(index);
  if (!cur) return false;
  if (cur->dump(0) == value.dump(0)) return true;
  project_->push(doc_, new SetValueCommand(doc_, pathOf(index), value));
  return true;
}

bool JsonTreeModel::addMember(const QModelIndex& parent, const QString& key, const ote::Json& value) {
  const ote::Json* c = jsonAt(parent);
  if (!c || !c->isObject()) { emit editRejected(tr("Keys can only be added inside an object.")); return false; }
  const std::string k = key.toStdString();
  if (k.empty()) { emit editRejected(tr("A key cannot be empty.")); return false; }
  if (c->has(k.c_str())) { emit editRejected(tr("\"%1\" already exists here.").arg(key)); return false; }
  project_->push(doc_, new InsertCommand(doc_, pathOf(parent), c->members().size(), k, value));
  return true;
}

bool JsonTreeModel::addItem(const QModelIndex& parent, const ote::Json& value) {
  const ote::Json* c = jsonAt(parent);
  if (!c || !c->isArray()) { emit editRejected(tr("Items can only be added inside an array.")); return false; }
  project_->push(doc_, new InsertCommand(doc_, pathOf(parent), c->items().size(), "", value));
  return true;
}

bool JsonTreeModel::removeNode(const QModelIndex& index) {
  if (!index.isValid()) return false;
  auto* cmd = new EraseCommand(doc_, pathOf(index));
  if (!cmd->valid()) { delete cmd; return false; }
  project_->push(doc_, cmd);
  return true;
}

bool JsonTreeModel::renameKey(const QModelIndex& index, const QString& newKey) {
  Node* n = nodeFor(index);
  if (!index.isValid() || n->inArray) return false;
  const std::string k = newKey.toStdString();
  if (k == n->key) return true;
  const ote::Json* parentJson = jsonAt(parent(index));
  const ote::Json* cur = jsonAt(index);
  if (!parentJson || !cur) return false;
  if (k.empty()) { emit editRejected(tr("A key cannot be empty.")); return false; }
  if (parentJson->has(k.c_str())) { emit editRejected(tr("\"%1\" already exists here.").arg(newKey)); return false; }
  const JsonPath container = pathOf(parent(index));
  const size_t at = (size_t)n->row;
  const ote::Json value = *cur;
  auto* erase = new EraseCommand(doc_, pathOf(index));
  QUndoStack* stack = project_->undoStack();
  stack->beginMacro(tr("Rename %1").arg(pathToString(pathOf(index))));
  project_->push(doc_, erase);
  project_->push(doc_, new InsertCommand(doc_, container, at, k, value));
  stack->endMacro();
  return true;
}

void JsonTreeModel::onAboutToChange(const JsonChange& c) {
  if (c.kind == JsonChange::Kind::Set) {
    if (c.path.empty()) { beginResetModel(); resetting_ = true; return; }
    Node* n = findNode(c.path);
    if (n && n->built && !n->kids.empty()) {
      // The node's children are about to be different ones: drop the old rows now, add the new ones when it is done.
      beginRemoveRows(indexOf(n), 0, (int)n->kids.size() - 1);
      n->kids.clear();
      endRemoveRows();
    }
    return;
  }
  Node* n = findNode(c.path);
  if (!n || !n->built) return;
  switch (c.kind) {
    case JsonChange::Kind::Insert: beginInsertRows(indexOf(n), (int)c.index, (int)c.index); break;
    case JsonChange::Kind::Erase: beginRemoveRows(indexOf(n), (int)c.index, (int)c.index); break;
    case JsonChange::Kind::Move: beginMoveRows(indexOf(n), (int)c.index, (int)c.index, indexOf(n), (int)(c.to > c.index ? c.to + 1 : c.to)); break;
    default: break;
  }
}

void JsonTreeModel::onChanged(const JsonChange& c) {
  if (resetting_) {
    resetting_ = false;
    root_ = std::make_unique<Node>();
    endResetModel();
    return;
  }
  Node* n = findNode(c.path);
  if (!n) return;
  const ote::Json* j = resolvePath(doc_->root(), c.path);
  auto refresh = [&](Node* node) {
    if (node != root_.get()) emit dataChanged(indexOf(node, 0), indexOf(node, ColumnCount - 1));
  };
  if (c.kind == JsonChange::Kind::Set) {
    if (n->built && j && (j->isArray() || j->isObject())) {
      const int count = (int)(j->isArray() ? j->items().size() : j->members().size());
      if (count > 0) {
        beginInsertRows(indexOf(n), 0, count - 1);
        for (int i = 0; i < count; ++i) n->kids.push_back(makeNode(n, *j, i));
        endInsertRows();
      }
    }
    refresh(n);
    return;
  }
  if (!n->built) { refresh(n); return; }  // never shown: it reads the new data when it is first asked
  switch (c.kind) {
    case JsonChange::Kind::Insert: {
      auto k = std::make_unique<Node>();
      k->parent = n;
      k->row = (int)c.index;
      k->inArray = j && j->isArray();
      k->key = c.key;
      n->kids.insert(n->kids.begin() + (std::ptrdiff_t)c.index, std::move(k));
      renumber(n, (int)c.index + 1);
      endInsertRows();
      break;
    }
    case JsonChange::Kind::Erase:
      n->kids.erase(n->kids.begin() + (std::ptrdiff_t)c.index);
      renumber(n, (int)c.index);
      endRemoveRows();
      break;
    case JsonChange::Kind::Move: {
      auto first = n->kids.begin();
      if (c.index < c.to) std::rotate(first + (std::ptrdiff_t)c.index, first + (std::ptrdiff_t)c.index + 1, first + (std::ptrdiff_t)c.to + 1);
      else std::rotate(first + (std::ptrdiff_t)c.to, first + (std::ptrdiff_t)c.index, first + (std::ptrdiff_t)c.index + 1);
      renumber(n, (int)std::min(c.index, c.to));
      endMoveRows();
      break;
    }
    default: break;
  }
  refresh(n);  // its summary ("{3 keys}") changed
}

}  // namespace ote_native
