// Event loader: showMap / returnMap, a scene on another map of the game (docs/EVENTS.md "Showing another map").
//
// The commands between a showMap and its returnMap act on the shown map's own temporary units, so the loader checks them against that map:
// while a section is read, the StoryLoader's map, unit ids, used ids and guard groups are those of the shown map (swapContext), and the
// battle's own are kept in `saved_`. A few readers that act on the real battle (an `if`'s conditions, a variable read from a unit,
// setPhaseMusic) switch back for their part (onRealMap).
#include <algorithm>

#include "core/data/loader.h"
#include "core/data/loader_content.h"
#include "core/data/loader_events.h"

namespace ote {

namespace {

std::string quoted(const std::string& s) { return "\"" + s + "\""; }

}  // namespace

void StoryLoader::swapContext() {
  std::swap(map_, saved_.map);
  std::swap(unitIds, saved_.unitIds);
  std::swap(usedIds, saved_.usedIds);
  std::swap(groups, saved_.groups);
  std::swap(actorOk_, saved_.actorOk);
}

bool StoryLoader::onRealMap(const std::function<bool()>& read) {
  if (!shown_) return read();
  swapContext();
  const bool ok = read();
  swapContext();
  return ok;
}

void StoryLoader::endShownMap() {
  if (!shown_) return;
  swapContext();
  shown_.reset();
  saved_ = Context();
}

bool StoryLoader::showMapCommand(const Json& e, const std::string& at, Command& out) {
  out.kind = Command::Kind::ShowMap;
  if (!onlyKeys(e, {"showMap", "focus"}, at)) return false;
  if (depth_ != 1 || !list_)
    return fail(at, "\"showMap\" goes in the event's own \"do\" list, not inside an \"if\" or the answer of a question (the map it shows "
                    "stays until the returnMap, so both must be at the top of the event)");
  if (shown_)
    return fail(at, "map " + quoted(shown_->id) + " is shown already: write { \"returnMap\": true } before showing another one");
  const Json& v = e["showMap"];
  if (!v.isString()) return fail(at, "\"showMap\" must be the id of a map in game.json \"maps\"");
  const std::string id = v.asString();
  out.index = db_.findMap(id);
  if (out.index < 0) return fail(at, "\"showMap\" names unknown map " + quoted(id) + " (game.json \"maps\")");
  if (id == realMap_->id)
    return fail(at, "\"showMap\" names this map itself; it shows another map of the game (a flashback, a place that is never played)");
  auto shown = std::make_shared<MapDef>();
  std::string why;
  if (!loadMapShape(db_, id, *shown, why)) return fail(at, "map " + quoted(id) + " cannot be shown, it does not load: " + why);

  // The shown map's context: its units' ids and guard groups, and the ids this section's addUnit commands create.
  Context ctx;
  ctx.map = shown.get();
  for (const MapUnitDef& u : shown->units) {
    if (u.id.empty()) continue;
    ctx.usedIds.push_back({u.id, u.onlyOn});
    if (std::find(ctx.unitIds.begin(), ctx.unitIds.end(), u.id) == ctx.unitIds.end()) ctx.unitIds.push_back(u.id);
  }
  for (const MapUnitDef& u : shown->units)
    if (!u.behaviour.group.empty()) ctx.groups.push_back(u.behaviour.group);
  collectShownUnitIds(*list_, at_ + 1, ctx.unitIds);
  saved_ = ctx;
  swapContext();  // from here on the commands read against the shown map; `saved_` holds the battle's own
  shown_ = shown;
  out.shownMap = shown;

  if (!e.has("focus")) return true;
  const Json& f = e["focus"];
  out.on = true;
  if (f.isString()) return unitId(f, at, "\"focus\"", out.unit);
  if (!f.isObject()) return fail(at, "\"focus\" must be a unit id of the shown map or a square { \"x\": 3, \"y\": 4 }");
  if (!onlyKeys(f, {"x", "y"}, at + ": \"focus\"") || !c_.integer(f, "x", at, out.x, true) || !c_.integer(f, "y", at, out.y, true))
    return false;
  if (out.x < 0 || out.y < 0 || out.x >= shown->width || out.y >= shown->height)
    return fail(at, "the square " + std::to_string(out.x) + "," + std::to_string(out.y) + " is outside map " + quoted(id));
  return true;
}

bool StoryLoader::returnMapCommand(const Json& e, const std::string& at, Command& out) {
  out.kind = Command::Kind::ReturnMap;
  if (!onlyKeys(e, {"returnMap"}, at)) return false;
  if (!e["returnMap"].isBool() || !e["returnMap"].asBool()) return fail(at, "write { \"returnMap\": true }");
  if (depth_ != 1) return fail(at, "\"returnMap\" goes in the event's own \"do\" list, like the showMap it ends");
  if (!shown_) return fail(at, "nothing to return from: no showMap comes before it in this event");
  endShownMap();
  return true;
}

}  // namespace ote
