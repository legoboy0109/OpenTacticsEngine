#include "json_commands.h"

#include <utility>

namespace ote_native {

JsonCommand::JsonCommand(JsonDocument* doc, const QString& text)
    : QUndoCommand(text), doc_(doc), generation_(doc->generation()) {}

void JsonCommand::redo() {
  if (doc_->generation() != generation_) return;  // the file was reloaded: this command belongs to a tree that is gone
  if (apply()) {
    applied_ = true;
    doc_->bumpVersion(+1);
  }
}

void JsonCommand::undo() {
  if (doc_->generation() != generation_ || !applied_) return;
  if (revert()) {
    applied_ = false;
    doc_->bumpVersion(-1);
  }
}

SetValueCommand::SetValueCommand(JsonDocument* doc, JsonPath path, ote::Json value)
    : JsonCommand(doc, QStringLiteral("Set %1").arg(pathToString(path))), path_(std::move(path)), new_(std::move(value)) {
  if (const ote::Json* n = resolvePath(doc->root(), path_)) old_ = *n;
}

bool SetValueCommand::apply() { return doc_->applySet(path_, new_); }
bool SetValueCommand::revert() { return doc_->applySet(path_, old_); }

InsertCommand::InsertCommand(JsonDocument* doc, JsonPath container, size_t index, std::string key, ote::Json value)
    : JsonCommand(doc, QStringLiteral("Insert into %1").arg(pathToString(container))),
      container_(std::move(container)), index_(index), key_(std::move(key)), value_(std::move(value)) {}

bool InsertCommand::apply() { return doc_->applyInsert(container_, index_, key_, value_); }
bool InsertCommand::revert() { return doc_->applyErase(container_, index_); }

EraseCommand::EraseCommand(JsonDocument* doc, const JsonPath& path)
    : JsonCommand(doc, QStringLiteral("Delete %1").arg(pathToString(path))) {
  if (path.empty()) return;  // the root cannot be erased
  container_ = JsonPath(path.begin(), path.end() - 1);
  const ote::Json* parent = resolvePath(doc->root(), container_);
  const ote::Json* node = resolvePath(doc->root(), path);
  if (!parent || !node) return;
  const PathStep& last = path.back();
  if (parent->isArray() && last.isIndex) {
    index_ = last.index;
  } else if (parent->isObject() && !last.isIndex) {
    bool found = false;
    for (size_t i = 0; i < parent->members().size(); ++i)
      if (parent->members()[i].first == last.key) { index_ = i; found = true; break; }
    if (!found) return;
    key_ = last.key;
  } else {
    return;
  }
  old_ = *node;
  valid_ = true;
}

bool EraseCommand::apply() { return valid_ && doc_->applyErase(container_, index_); }
bool EraseCommand::revert() { return doc_->applyInsert(container_, index_, key_, old_); }

MoveCommand::MoveCommand(JsonDocument* doc, JsonPath container, size_t from, size_t to)
    : JsonCommand(doc, QStringLiteral("Move in %1").arg(pathToString(container))), container_(std::move(container)), from_(from), to_(to) {}

bool MoveCommand::apply() { return doc_->applyMove(container_, from_, to_); }
bool MoveCommand::revert() { return doc_->applyMove(container_, to_, from_); }

}  // namespace ote_native
