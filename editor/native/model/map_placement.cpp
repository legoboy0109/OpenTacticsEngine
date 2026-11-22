#include "map_placement.h"

#include <QCoreApplication>
#include <QUndoStack>

#include "json_commands.h"
#include "project.h"

namespace ote_native {

using ote::Json;

namespace {
QString tr(const char* s) { return QCoreApplication::translate("MapPlacement", s); }

JsonPath slotPath(size_t i) { return {PathStep::member("prep"), PathStep::member("slots"), PathStep::at(i)}; }

bool inside(int x, int y, int w, int h) { return x >= 0 && y >= 0 && x < w && y < h; }

int findAt(const Json& list, int x, int y, long skip = -1) {
  for (size_t i = 0; i < list.size(); ++i) {
    const Json& e = list[i];
    if ((long)i != skip && e["x"].isNumber() && e["y"].isNumber() && e["x"].asInt() == x && e["y"].asInt() == y) return (int)i;
  }
  return -1;
}

// Runs `edits` as one undo step.
template <class F>
void macro(Project* p, const QString& text, F&& edits) {
  p->undoStack()->beginMacro(text);
  edits();
  p->undoStack()->endMacro();
}
}  // namespace

int MapPlacement::slotAt(int x, int y) const { return findAt(doc_->root()["prep"]["slots"], x, y); }
int MapPlacement::unitAt(int x, int y) const { return findAt(doc_->root()["units"], x, y); }

bool MapPlacement::addSlot(int x, int y, int w, int h, QString* err) {
  const Json& root = doc_->root();
  if (!inside(x, y, w, h)) return false;
  if (slotAt(x, y) >= 0) {
    if (err) *err = tr("There is already a deployment slot on this square.");
    return false;
  }
  Json slot = Json::makeObject();
  slot.set("x", Json::makeNumber(x));
  slot.set("y", Json::makeNumber(y));
  const Json& prep = root["prep"];
  if (prep.isObject() && prep["slots"].isArray() && (int)prep["slots"].size() >= kMaxSlots) {
    if (err) *err = tr("A map can have at most 64 deployment slots.");
    return false;
  }
  macro(project_, tr("Add deployment slot"), [&] {
    if (!prep.isObject()) {
      Json list = Json::makeArray();
      list.push(slot);
      Json p = Json::makeObject();
      p.set("slots", list);
      project_->push(doc_, new InsertCommand(doc_, {}, root.members().size(), "prep", p));
    } else if (!prep["slots"].isArray()) {
      Json list = Json::makeArray();
      list.push(slot);
      project_->push(doc_, new InsertCommand(doc_, {PathStep::member("prep")}, 0, "slots", list));
    } else {
      project_->push(doc_, new InsertCommand(doc_, {PathStep::member("prep"), PathStep::member("slots")}, prep["slots"].size(), "", slot));
    }
  });
  return true;
}

bool MapPlacement::moveSlot(size_t index, int x, int y, int w, int h, QString* err) {
  const Json& list = doc_->root()["prep"]["slots"];
  if (index >= list.size() || !inside(x, y, w, h)) return false;
  if (list[index]["x"].asInt() == x && list[index]["y"].asInt() == y) return false;
  if (findAt(list, x, y, (long)index) >= 0) {
    if (err) *err = tr("There is already a deployment slot on this square.");
    return false;
  }
  macro(project_, tr("Move deployment slot"), [&] {
    JsonPath px = slotPath(index), py = px;
    px.push_back(PathStep::member("x"));
    py.push_back(PathStep::member("y"));
    project_->push(doc_, new SetValueCommand(doc_, px, Json::makeNumber(x)));
    project_->push(doc_, new SetValueCommand(doc_, py, Json::makeNumber(y)));
  });
  return true;
}

bool MapPlacement::removeSlot(size_t index, QString* err) {
  const Json& prep = doc_->root()["prep"];
  const Json& list = prep["slots"];
  if (index >= list.size()) return false;
  if (list.size() == 1) {
    if (err) *err = tr("A preparation screen needs at least one slot. To switch the screen off, delete the map's \"prep\" in the Tree page.");
    return false;
  }
  const long number = (long)index + 1;  // slot numbers in "required" count from 1
  macro(project_, tr("Remove deployment slot"), [&] {
    const Json& req = prep["required"];
    for (size_t i = 0; i < req.size(); ++i) {
      if (!req[i].isObject() || !req[i]["slot"].isNumber()) continue;
      const long n = (long)req[i]["slot"].asInt();
      JsonPath p = {PathStep::member("prep"), PathStep::member("required"), PathStep::at(i)};
      if (n == number) {
        // This character's own slot is gone: it stays required, with no fixed square. Written as the object minus "slot".
        Json o = Json::makeObject();
        for (const auto& kv : req[i].members())
          if (kv.first != "slot") o.set(kv.first, kv.second);
        project_->push(doc_, new SetValueCommand(doc_, p, o));
      } else if (n > number) {
        p.push_back(PathStep::member("slot"));
        project_->push(doc_, new SetValueCommand(doc_, p, Json::makeNumber((double)(n - 1))));
      }
    }
    if (prep["limit"].isNumber() && prep["limit"].asInt() > (int)list.size() - 1)
      project_->push(doc_, new SetValueCommand(doc_, {PathStep::member("prep"), PathStep::member("limit")}, Json::makeNumber((double)list.size() - 1)));
    project_->push(doc_, new EraseCommand(doc_, slotPath(index)));
  });
  return true;
}

bool MapPlacement::moveUnit(size_t index, int x, int y, int w, int h, QString* err) {
  const Json& list = doc_->root()["units"];
  if (index >= list.size() || !inside(x, y, w, h)) return false;
  if (list[index]["x"].asInt() == x && list[index]["y"].asInt() == y) return false;
  if (findAt(list, x, y, (long)index) >= 0) {
    if (err) *err = tr("There is already a unit on this square.");
    return false;
  }
  macro(project_, tr("Move unit"), [&] {
    JsonPath px = {PathStep::member("units"), PathStep::at(index), PathStep::member("x")}, py = px;
    py.back() = PathStep::member("y");
    project_->push(doc_, new SetValueCommand(doc_, px, Json::makeNumber(x)));
    project_->push(doc_, new SetValueCommand(doc_, py, Json::makeNumber(y)));
  });
  return true;
}

}  // namespace ote_native
