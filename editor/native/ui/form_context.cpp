#include "form_context.h"

#include <QUndoStack>

#include "json_commands.h"
#include "json_text_dialog.h"
#include "project.h"
#include "story_ids.h"
#include "variant_picker_dialog.h"

namespace ote_native {

FormContext::FormContext(Project* project, JsonDocument* doc, IdSource* ids, QObject* parent)
    : QObject(parent), project_(project), doc_(doc), ids_(ids) {}

IdLookup FormContext::lookup() const {
  return [this](const std::vector<std::string>& s) { return idsOf(s); };
}

QStringList FormContext::idsOf(const std::vector<std::string>& sources) const { return resolveSources(doc_->root(), ids_, sources); }

const ote::Json* FormContext::at(const JsonPath& path) const { return resolvePath(doc_->root(), path); }

bool FormContext::set(const JsonPath& path, const ote::Json& value, const QString& undoText) {
  if (path.empty()) return false;
  if (const ote::Json* cur = at(path)) {
    if (cur->dump(0) == value.dump(0)) return true;
    project_->push(doc_, new SetValueCommand(doc_, path, value));
    return true;
  }
  // Absent: find the deepest ancestor that exists; everything below it is created as objects.
  size_t have = path.size() - 1;
  while (have > 0 && !at(JsonPath(path.begin(), path.begin() + (std::ptrdiff_t)have))) --have;
  const ote::Json* host = at(JsonPath(path.begin(), path.begin() + (std::ptrdiff_t)have));
  if (!host || !host->isObject()) {
    emit refused(tr("Cannot set %1: %2 is not an object in this file.").arg(pathToString(path), pathToString(JsonPath(path.begin(), path.begin() + (std::ptrdiff_t)have))));
    return false;
  }
  for (size_t i = have; i < path.size(); ++i)
    if (path[i].isIndex) {
      emit refused(tr("Cannot set %1: the list item does not exist.").arg(pathToString(path)));
      return false;
    }
  QUndoStack* stack = project_->undoStack();
  const bool macro = have + 1 < path.size();
  if (macro) stack->beginMacro(undoText.isEmpty() ? tr("Set %1").arg(pathToString(path)) : undoText);
  JsonPath container(path.begin(), path.begin() + (std::ptrdiff_t)have);
  for (size_t i = have; i < path.size(); ++i) {
    const ote::Json* c = at(container);
    const bool last = i + 1 == path.size();
    project_->push(doc_, new InsertCommand(doc_, container, c->members().size(), path[i].key, last ? value : ote::Json::makeObject()));
    container.push_back(path[i]);
  }
  if (macro) stack->endMacro();
  return true;
}

bool FormContext::setField(const SchemaNode& node, const JsonPath& path, const ote::Json& value) {
  std::vector<JsonPath> drop;
  for (const std::string& k : node.excludes) {
    JsonPath sib(path.begin(), path.end() - 1);
    sib.push_back(PathStep::member(k));
    if (at(sib)) drop.push_back(sib);
  }
  if (drop.empty()) return set(path, value);
  QUndoStack* stack = project_->undoStack();
  stack->beginMacro(tr("Set %1").arg(pathToString(path)));
  for (const JsonPath& d : drop) erase(d);
  const bool ok = set(path, value);
  stack->endMacro();
  return ok;
}

bool FormContext::erase(const JsonPath& path) {
  auto* cmd = new EraseCommand(doc_, path);
  if (!cmd->valid()) {
    delete cmd;
    return false;
  }
  project_->push(doc_, cmd);
  return true;
}

bool FormContext::appendItem(const JsonPath& list, const ote::Json& item) {
  const ote::Json* l = at(list);
  if (!l) {
    ote::Json arr = ote::Json::makeArray();
    arr.push(item);
    return set(list, arr);
  }
  if (!l->isArray()) return false;
  project_->push(doc_, new InsertCommand(doc_, list, l->size(), "", item));
  return true;
}

bool FormContext::moveItem(const JsonPath& list, size_t from, size_t to) {
  const ote::Json* l = at(list);
  if (!l || !l->isArray() || from >= l->size() || to >= l->size() || from == to) return false;
  project_->push(doc_, new MoveCommand(doc_, list, from, to));
  return true;
}

bool FormContext::insertItem(const JsonPath& list, size_t index, const ote::Json& item) {
  const ote::Json* l = at(list);
  if (!l || !l->isArray() || index > l->size()) return false;
  project_->push(doc_, new InsertCommand(doc_, list, index, "", item));
  return true;
}

int FormContext::pickVariant(const QString& title, const std::vector<std::pair<QString, QString>>& entries, QWidget* parent) const {
  if (variantPicker_) return variantPicker_(title, entries);
  return VariantPickerDialog::pick(title, entries, parent);
}

bool FormContext::editJson(const QString& title, const ote::Json& initial, ote::Json* result, QWidget* parent) const {
  if (jsonEditor_) return jsonEditor_(title, initial, result);
  JsonTextDialog dlg(title, initial, parent);
  if (dlg.exec() != QDialog::Accepted) return false;
  *result = dlg.value();
  return true;
}

}  // namespace ote_native
