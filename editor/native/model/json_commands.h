// The edit operations on a JsonDocument, as QUndoCommands for the project's single QUndoStack:
// set a value, insert a child, erase a child, move a child within its container. Each remembers what it needs to undo
// itself and counts itself into the document's version, which is how dirty tracking survives undo back to the saved state.
// A command created for an older load of the file (the file was reloaded since) does nothing.
#pragma once
#include <QUndoCommand>

#include "json_document.h"

namespace ote_native {

class JsonCommand : public QUndoCommand {
 public:
  void undo() final;
  void redo() final;

 protected:
  JsonCommand(JsonDocument* doc, const QString& text);
  virtual bool apply() = 0;
  virtual bool revert() = 0;
  JsonDocument* doc_;

 private:
  int generation_;
  bool applied_ = false;
};

// Replace the node at `path` (which must exist) with `value`.
class SetValueCommand : public JsonCommand {
 public:
  SetValueCommand(JsonDocument* doc, JsonPath path, ote::Json value);

 protected:
  bool apply() override;
  bool revert() override;

 private:
  JsonPath path_;
  ote::Json new_, old_;
};

// Insert `value` into the array or object at `container` at position `index` (objects: under `key`).
class InsertCommand : public JsonCommand {
 public:
  InsertCommand(JsonDocument* doc, JsonPath container, size_t index, std::string key, ote::Json value);

 protected:
  bool apply() override;
  bool revert() override;

 private:
  JsonPath container_;
  size_t index_;
  std::string key_;
  ote::Json value_;
};

// Remove the node at `path` (the last step says which child of its container).
class EraseCommand : public JsonCommand {
 public:
  EraseCommand(JsonDocument* doc, const JsonPath& path);
  bool valid() const { return valid_; }

 protected:
  bool apply() override;
  bool revert() override;

 private:
  JsonPath container_;
  size_t index_ = 0;
  std::string key_;
  ote::Json old_;
  bool valid_ = false;
};

// Move a child of `container` from `from` to `to` (the position it has afterwards).
class MoveCommand : public JsonCommand {
 public:
  MoveCommand(JsonDocument* doc, JsonPath container, size_t from, size_t to);

 protected:
  bool apply() override;
  bool revert() override;

 private:
  JsonPath container_;
  size_t from_, to_;
};

}  // namespace ote_native
