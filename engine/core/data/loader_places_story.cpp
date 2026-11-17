// Event loader, doors / chests / places (docs/SRPG.md section 10; docs/EVENTS.md): the "place" trigger and the commands
// openDoor, closeDoor, lockDoor, unlockDoor and openChest. Self-contained so the shared story loader files change by a line each.
#include "core/data/loader_events.h"

namespace ote {

namespace {
std::string q(const std::string& s) { return "\"" + s + "\""; }
}  // namespace

// The map's structure with this id, -1 if there is none.
static int placeIndex(const MapDef& map, const std::string& id) {
  for (size_t i = 0; i < map.structures.size(); ++i)
    if (map.structures[i].id == id) return (int)i;
  return -1;
}

bool StoryLoader::placeTrigger(const Json& w, const std::string& at, Trigger& t) {
  t.kind = Trigger::Kind::Place;
  if (!onlyKeys(w, {"place", "is", "by"}, at)) return false;
  if (!w["place"].isString() || w["place"].asString().empty()) return fail(at, "\"place\" must be the id of a door, chest or place of the map");
  t.place = w["place"].asString();
  const int i = placeIndex(map_, t.place);
  if (i < 0) return fail(at, "\"place\" names " + q(t.place) + ", which is not the \"id\" of a structure of this map");
  const PlaceDef& p = db_.structures[(size_t)map_.structures[(size_t)i].type].place;
  if (!w.has("is") || !w["is"].isString()) return fail(at, "a place trigger needs \"is\": \"opened\", \"visited\" or \"destroyed\"");
  const std::string is = w["is"].asString();
  if (is == "opened") {
    t.placeIs = kPlaceOpened;
    if (!p.isDoor() && !p.isChest()) return fail(at, q(t.place) + " is not a door or chest, so it is not \"opened\" (use \"visited\" or \"destroyed\")");
  } else if (is == "visited") {
    t.placeIs = kPlaceVisited;
    if (!p.isVisit()) return fail(at, q(t.place) + " is not a place that can be visited (a kind with a \"visit\" block)");
  } else if (is == "destroyed") {
    t.placeIs = kPlaceDestroyed;
    if (!p.isVisit() || p.destroyedTo < 0) return fail(at, q(t.place) + " cannot be destroyed (its kind's \"visit\" needs a \"ruin\")");
  } else {
    return fail(at, "\"is\" must be \"opened\", \"visited\" or \"destroyed\", not " + q(is));
  }
  return !w.has("by") || filter(w["by"], at, "\"by\"", t.who);
}

bool StoryLoader::placeCommand(const Json& e, const std::string& kind, const std::string& at, Command& out) {
  out.kind = Command::Kind::Place;
  const bool chest = kind == "openChest";
  if (!onlyKeys(e, chest ? std::initializer_list<const char*>{"openChest", "by"} : std::initializer_list<const char*>{kind.c_str()}, at))
    return false;
  const Json& v = e[kind.c_str()];
  if (!v.isString() || v.asString().empty()) return fail(at, q(kind) + " must be the id of a " + (chest ? "chest" : "door") + " of the map");
  out.place = v.asString();
  const int i = placeIndex(map_, out.place);
  if (i < 0) return fail(at, q(kind) + " names " + q(out.place) + ", which is not the \"id\" of a structure of this map");
  const PlaceDef& p = db_.structures[(size_t)map_.structures[(size_t)i].type].place;
  if (chest && !p.isChest()) return fail(at, q(out.place) + " is not a chest");
  if (!chest && !p.isDoor() && !(kind == "unlockDoor" && p.isChest()))
    return fail(at, q(out.place) + " is not a door" + (kind == "unlockDoor" ? " or chest" : ""));
  if (kind == "openDoor") out.index = kPlaceCmdOpenDoor;
  else if (kind == "closeDoor") out.index = kPlaceCmdCloseDoor;
  else if (kind == "lockDoor") out.index = kPlaceCmdLockDoor;
  else if (kind == "unlockDoor") out.index = kPlaceCmdUnlockDoor;
  else out.index = kPlaceCmdOpenChest;
  if (chest && e.has("by")) {
    if (!unitId(e["by"], at, "\"by\"", out.unit)) return false;  // the unit that gets the contents
  }
  return true;
}

}  // namespace ote
