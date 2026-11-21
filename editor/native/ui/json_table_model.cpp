#include "json_table_model.h"

#include <QFont>
#include <QSet>
#include <algorithm>

#include <QApplication>
#include <QIcon>
#include <QStyle>

#include "cell_text.h"
#include "id_source.h"
#include "json_commands.h"
#include "project.h"
#include "table_columns.h"

namespace ote_native {

namespace {
bool sameJson(const ote::Json& a, const ote::Json& b) { return a.dump(0) == b.dump(0); }

// The member `key` of an object, or nullptr (the Json lookups return a shared Null, which cannot tell "absent" from null).
const ote::Json* member(const ote::Json* obj, const std::string& key) {
  if (!obj || !obj->isObject()) return nullptr;
  for (const auto& m : obj->members())
    if (m.first == key) return &m.second;
  return nullptr;
}
}  // namespace

QString JsonTableModel::columnKey(int col) const {
  QStringList parts;
  for (const std::string& s : columns_[(size_t)col].path) parts << QString::fromStdString(s);
  return parts.join('.');
}

void JsonTableModel::setSchema(const SchemaNode* rows, IdSource* ids) {
  rowsSchema_ = rows;
  ids_ = ids;
  beginResetModel();
  rebuildColumns();
  endResetModel();
  if (ids_) connect(ids_, &IdSource::idsChanged, this, &JsonTableModel::refreshPlan, Qt::UniqueConnection);
}

void JsonTableModel::refreshPlan() {
  if (!rowsSchema_) return;
  const std::vector<ColumnSpec> now = planColumns(*rowsSchema_, ids_);
  bool same = now.size() == plan_.size();
  for (size_t i = 0; same && i < now.size(); ++i) same = now[i].path == plan_[i].path;
  if (same) return;
  beginResetModel();
  rebuildColumns();
  endResetModel();
}

int JsonTableModel::columnOfKey(const QString& key) const {
  const std::string k = key.toStdString();
  for (size_t i = 0; i < columns_.size(); ++i)
    if (columns_[i].path.front() == k) return (int)i;
  return -1;
}

bool JsonTableModel::cellMismatch(int row, int col, QString* why) const {
  const SchemaNode* n = columns_[(size_t)col].node;
  const ote::Json* v = cellValue(row, col);
  return n && v && !nodeAccepts(*n, *v, why);
}

bool JsonTableModel::covered(const std::string& key) const {
  for (const ColumnSpec& c : columns_)
    if (c.path.front() == key) return true;
  return false;
}

JsonTableModel::JsonTableModel(Project* project, JsonDocument* doc, JsonPath arrayPath, QObject* parent)
    : QAbstractTableModel(parent), project_(project), doc_(doc), arrayPath_(std::move(arrayPath)) {
  rebuildColumns();
  connect(doc_, &JsonDocument::aboutToChange, this, &JsonTableModel::onAboutToChange);
  connect(doc_, &JsonDocument::changed, this, &JsonTableModel::onChanged);
  connect(doc_, &JsonDocument::reloaded, this, [this] {
    beginResetModel();
    rebuildColumns();
    endResetModel();
  });
}

const ote::Json* JsonTableModel::array() const {
  const ote::Json* a = resolvePath(doc_->root(), arrayPath_);
  return (a && a->isArray()) ? a : nullptr;
}

void JsonTableModel::rebuildColumns() {
  columns_.clear();
  plan_.clear();
  if (rowsSchema_) {
    plan_ = planColumns(*rowsSchema_, ids_);
    columns_ = plan_;
  }
  growColumns();
}

bool JsonTableModel::growColumns() {
  const size_t before = columns_.size();
  const ote::Json* a = array();
  if (!a) return false;
  for (const ote::Json& row : a->items()) {
    if (!row.isObject()) continue;
    for (const auto& m : row.members()) {
      if (!covered(m.first)) {
        ColumnSpec c;
        c.path = {m.first};
        c.hint = m.second.type();
        columns_.push_back(c);
      } else {
        // A column's usual type is the first non-null one, so a leading null does not hide it.
        for (ColumnSpec& c : columns_)
          if (c.path.size() == 1 && c.path[0] == m.first && c.hint == ote::Json::Type::Null) c.hint = m.second.type();
      }
    }
  }
  return columns_.size() != before;
}

int JsonTableModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) return 0;
  const ote::Json* a = array();
  return a ? (int)a->items().size() : 0;
}

int JsonTableModel::columnCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : (int)columns_.size(); }

const ote::Json* JsonTableModel::rowValue(int row) const {
  const ote::Json* a = array();
  if (!a || row < 0 || (size_t)row >= a->items().size()) return nullptr;
  return &a->items()[(size_t)row];
}

const ote::Json* JsonTableModel::cellValue(int row, int col) const {
  const ote::Json* r = rowValue(row);
  if (!r || !r->isObject() || col < 0 || (size_t)col >= columns_.size()) return nullptr;
  const ote::Json* v = r;
  for (const std::string& step : columns_[(size_t)col].path) {
    v = member(v, step);
    if (!v) return nullptr;
  }
  return v;
}

bool JsonTableModel::isNestedCell(int row, int col) const {
  const ote::Json* v = cellValue(row, col);
  return v && (v->isObject() || v->isArray());
}

QVariant JsonTableModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid()) return {};
  const ote::Json* v = cellValue(index.row(), index.column());
  switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
      return v ? QVariant(jsonToCellText(*v)) : QVariant(QString());
    case Qt::CheckStateRole:
      if (v && v->isBool()) return v->asBool() ? Qt::Checked : Qt::Unchecked;
      return {};
    case Qt::ToolTipRole: {
      QString why;
      if (cellMismatch(index.row(), index.column(), &why))
        return QStringLiteral("Does not match the schema: %1. The value is kept as it is.").arg(why);
      if (v && (v->isObject() || v->isArray())) return QString::fromStdString(v->dump(2));
      if (const SchemaNode* n = columns_[(size_t)index.column()].node) return QString::fromStdString(n->help);
      return {};
    }
    case Qt::DecorationRole:
      if (cellMismatch(index.row(), index.column())) return QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning);
      return {};
    case Qt::FontRole:
      if (v && (v->isObject() || v->isArray())) {
        QFont f;
        f.setItalic(true);  // "this is JSON, double-click to edit": no colour, so every palette stays readable
        return f;
      }
      return {};
    case Qt::TextAlignmentRole:
      if (v && v->isNumber()) return QVariant(int(Qt::AlignRight | Qt::AlignVCenter));
      return {};
    case SortRole:
      if (!v) return {};
      if (v->isNumber()) return v->asNumber();
      if (v->isBool()) return v->asBool() ? 1 : 0;
      return jsonToCellText(*v);
    default:
      return {};
  }
}

Qt::ItemFlags JsonTableModel::flags(const QModelIndex& index) const {
  Qt::ItemFlags f = QAbstractTableModel::flags(index);
  if (!index.isValid()) return f;
  const ote::Json* v = cellValue(index.row(), index.column());
  if (v && v->isBool()) f |= Qt::ItemIsUserCheckable;
  // Nested cells are edited in the JSON dialog, not inline.
  if (!(v && (v->isObject() || v->isArray()))) f |= Qt::ItemIsEditable;
  return f;
}

QVariant JsonTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (role != Qt::DisplayRole) return {};
  if (orientation == Qt::Horizontal) return section >= 0 && (size_t)section < columns_.size() ? columnKey(section) : QVariant();
  return section + 1;
}

JsonPath JsonTableModel::rowPath(int row) const {
  JsonPath p = arrayPath_;
  p.push_back(PathStep::at((size_t)row));
  return p;
}

bool JsonTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
  if (!index.isValid()) return false;
  QString text;
  if (role == Qt::CheckStateRole) text = value.toInt() == Qt::Checked ? "true" : "false";
  else if (role == Qt::EditRole) text = value.toString();
  else return false;
  QString err;
  if (!applyTextEdits({{index.row(), index.column(), text}}, &err)) {
    emit editRejected(err);
    return false;
  }
  return true;
}

void JsonTableModel::setCellValue(int row, int col, const ote::Json& value) {
  const ote::Json* cur = cellValue(row, col);
  if (cur && sameJson(*cur, value)) return;
  JsonPath p = rowPath(row);
  for (const std::string& step : columns_[(size_t)col].path) p.push_back(PathStep::member(step));
  if (cur) {
    project_->push(doc_, new SetValueCommand(doc_, p, value));
    return;
  }
  // Absent: insert the key, creating the nested object it lives in first ("stats" of a row that has none yet).
  QUndoStack* stack = project_->undoStack();
  const bool macro = columns_[(size_t)col].path.size() > 1 && !member(rowValue(row), columns_[(size_t)col].path[0]);
  if (macro) stack->beginMacro(QStringLiteral("Set %1").arg(pathToString(p)));
  JsonPath parent(p.begin(), p.end() - 1);
  if (macro) project_->push(doc_, new InsertCommand(doc_, rowPath(row), rowValue(row)->members().size(), columns_[(size_t)col].path[0], ote::Json::makeObject()));
  if (const ote::Json* host = resolvePath(doc_->root(), parent))
    project_->push(doc_, new InsertCommand(doc_, parent, host->members().size(), p.back().key, value));
  if (macro) stack->endMacro();
}

bool JsonTableModel::applyTextEdits(const QList<CellEdit>& edits, QString* error) {
  struct Ready { int row, col; ote::Json value; };
  std::vector<Ready> ready;
  for (const CellEdit& e : edits) {
    if (!rowValue(e.row) || e.col < 0 || (size_t)e.col >= columns_.size()) continue;
    const ote::Json* cur = cellValue(e.row, e.col);
    // A blank pasted over a number/yes-no cell, or into an absent cell, means "nothing here": leave it alone.
    if (edits.size() > 1 && e.text.isEmpty() && (!cur || !cur->isString())) continue;
    if (!cur && e.text.isEmpty()) continue;
    CellParse p = parseCellText(e.text, cur, columnHint(e.col));
    if (!p.ok) {
      if (error) *error = QStringLiteral("Row %1, \"%2\": %3").arg(e.row + 1).arg(columnKey(e.col), p.error);
      return false;
    }
    ready.push_back({e.row, e.col, std::move(p.value)});
  }
  if (ready.empty()) return true;
  QUndoStack* stack = project_->undoStack();
  if (ready.size() > 1) stack->beginMacro(QStringLiteral("Paste %1 cells").arg(ready.size()));
  for (Ready& r : ready) setCellValue(r.row, r.col, r.value);
  if (ready.size() > 1) stack->endMacro();
  return true;
}

void JsonTableModel::clearCells(const QList<QPoint>& cells) {
  std::vector<JsonPath> paths;
  for (const QPoint& c : cells)
    if (cellValue(c.y(), c.x())) {
      JsonPath p = rowPath(c.y());
      for (const std::string& step : columns_[(size_t)c.x()].path) p.push_back(PathStep::member(step));
      paths.push_back(std::move(p));
    }
  if (paths.empty()) return;
  QUndoStack* stack = project_->undoStack();
  stack->beginMacro(QStringLiteral("Clear %1 cells").arg(paths.size()));
  for (const JsonPath& p : paths) {
    auto* cmd = new EraseCommand(doc_, p);
    if (cmd->valid()) project_->push(doc_, cmd);
    else delete cmd;
  }
  stack->endMacro();
}

void JsonTableModel::addRow(int at) {
  at = std::clamp(at, 0, rowCount());
  ote::Json row = ote::Json::makeObject();
  for (const ColumnSpec& c : columns_) {
    ote::Json d = defaultValueFor(c.hint);
    if (d.isNull()) continue;
    if (c.path.size() == 1) {
      row.set(c.path[0], d);
    } else {
      // A nested member: make the parent object (once) and give the member its default, so the new row is complete.
      ote::Json parent = member(&row, c.path[0]) ? *member(&row, c.path[0]) : ote::Json::makeObject();
      parent.set(c.path[1], d);
      row.set(c.path[0], parent);
    }
  }
  project_->push(doc_, new InsertCommand(doc_, arrayPath_, (size_t)at, "", row));
}

std::string JsonTableModel::uniqueId(const ote::Json& row, int) const {
  QSet<QString> taken;
  if (const ote::Json* a = array())
    for (const ote::Json& r : a->items())
      if (r["id"].isString()) taken.insert(QString::fromStdString(r["id"].asString()));
  const QString base = QString::fromStdString(row["id"].asString()) + "_copy";
  QString candidate = base;
  for (int n = 2; taken.contains(candidate); ++n) candidate = base + QString::number(n);
  taken.insert(candidate);
  return candidate.toStdString();
}

void JsonTableModel::duplicateRows(const QList<int>& rows) {
  QList<int> sorted = rows;
  std::sort(sorted.begin(), sorted.end());
  if (sorted.isEmpty()) return;
  QUndoStack* stack = project_->undoStack();
  stack->beginMacro(QStringLiteral("Duplicate %1 rows").arg(sorted.size()));
  int at = sorted.last() + 1;
  for (int r : sorted) {
    const ote::Json* src = rowValue(r);
    if (!src) continue;
    ote::Json copy = *src;
    if (copy["id"].isString()) copy.set("id", ote::Json::makeString(uniqueId(copy, r)));  // set() keeps the key's position
    project_->push(doc_, new InsertCommand(doc_, arrayPath_, (size_t)at++, "", copy));
  }
  stack->endMacro();
}

void JsonTableModel::deleteRows(const QList<int>& rows) {
  QList<int> sorted = rows;
  std::sort(sorted.begin(), sorted.end(), std::greater<int>());
  sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
  if (sorted.isEmpty()) return;
  QUndoStack* stack = project_->undoStack();
  stack->beginMacro(QStringLiteral("Delete %1 rows").arg(sorted.size()));
  for (int r : sorted) {
    auto* cmd = new EraseCommand(doc_, rowPath(r));
    if (cmd->valid()) project_->push(doc_, cmd);
    else delete cmd;
  }
  stack->endMacro();
}

void JsonTableModel::moveRows(const QList<int>& rowsAscending, int delta) {
  if (rowsAscending.isEmpty() || (delta != -1 && delta != 1)) return;
  const int n = rowCount();
  if (delta < 0 && rowsAscending.first() <= 0) return;
  if (delta > 0 && rowsAscending.last() >= n - 1) return;
  QUndoStack* stack = project_->undoStack();
  stack->beginMacro(QStringLiteral("Move %1 rows").arg(rowsAscending.size()));
  // Moving the leading row first when going up (the trailing one when going down) never steps on a row that has not moved.
  if (delta < 0) for (int r : rowsAscending) project_->push(doc_, new MoveCommand(doc_, arrayPath_, (size_t)r, (size_t)(r - 1)));
  else for (int i = rowsAscending.size() - 1; i >= 0; --i) project_->push(doc_, new MoveCommand(doc_, arrayPath_, (size_t)rowsAscending[i], (size_t)(rowsAscending[i] + 1)));
  stack->endMacro();
}

void JsonTableModel::onAboutToChange(const JsonChange& c) {
  if (pathStartsWith(arrayPath_, c.path) && c.path.size() < arrayPath_.size()) {  // an ancestor of our array changed
    beginResetModel();
    resetting_ = true;
    return;
  }
  if (!pathStartsWith(c.path, arrayPath_)) return;
  const bool onArray = c.path.size() == arrayPath_.size();
  switch (c.kind) {
    case JsonChange::Kind::Set:
      if (onArray) { beginResetModel(); resetting_ = true; }
      break;
    case JsonChange::Kind::Insert:
      if (onArray) beginInsertRows({}, (int)c.index, (int)c.index);
      break;
    case JsonChange::Kind::Erase:
      if (onArray) beginRemoveRows({}, (int)c.index, (int)c.index);
      break;
    case JsonChange::Kind::Move:
      if (onArray) beginMoveRows({}, (int)c.index, (int)c.index, {}, (int)(c.to > c.index ? c.to + 1 : c.to));
      break;
  }
}

void JsonTableModel::onChanged(const JsonChange& c) {
  if (resetting_) {
    resetting_ = false;
    rebuildColumns();
    endResetModel();
    return;
  }
  if (!pathStartsWith(c.path, arrayPath_)) return;
  const size_t depth = c.path.size() - arrayPath_.size();  // 0: the array itself, 1: a row, 2+: inside a cell
  auto columnReset = [this] {
    if (!growColumns()) return false;
    beginResetModel();
    endResetModel();
    return true;
  };
  if (depth == 0) {
    switch (c.kind) {
      case JsonChange::Kind::Insert: endInsertRows(); columnReset(); break;
      case JsonChange::Kind::Erase: endRemoveRows(); break;
      case JsonChange::Kind::Move: endMoveRows(); break;
      case JsonChange::Kind::Set: break;  // handled by resetting_
    }
    return;
  }
  const int row = (int)c.path[arrayPath_.size()].index;
  if (depth == 1) {
    if (c.kind == JsonChange::Kind::Set) {  // a whole row replaced
      if (!columnReset()) emit dataChanged(index(row, 0), index(row, std::max(0, columnCount() - 1)));
      return;
    }
    if (c.kind == JsonChange::Kind::Move) return;  // key order inside a row does not change the columns
    if (columnReset()) return;
    changedKey(row, c.key);
    return;
  }
  const PathStep& keyStep = c.path[arrayPath_.size() + 1];
  changedKey(row, keyStep.key);
}

// Every column that starts with `key` may show something new (a nested column when only part of its object changed).
void JsonTableModel::changedKey(int row, const std::string& key) {
  for (size_t i = 0; i < columns_.size(); ++i)
    if (columns_[i].path.front() == key) {
      const QModelIndex ix = index(row, (int)i);
      emit dataChanged(ix, ix);
    }
}

}  // namespace ote_native
