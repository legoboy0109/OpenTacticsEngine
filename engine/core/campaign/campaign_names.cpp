// Names the player typed for campaign characters (docs/EVENTS.md "Naming a character"): kept by the playthrough, put into its
// rosters (so the next battle, the panels and the camp use them) and saved as "names" (docs/SAVES.md).
#include <set>

#include "core/battle/save_reader.h"
#include "core/campaign/campaign_save.h"
#include "core/campaign/playthrough.h"
#include "core/data/name_tokens.h"

namespace ote {

const std::string* typedName(const Playthrough& pt, const std::string& id) {
  for (const auto& n : pt.names)
    if (n.first == id) return &n.second;
  return nullptr;
}

bool setTypedName(Playthrough& pt, const std::string& id, const std::string& name) {
  if (!isStoryId(id) || !validEnteredName(name)) return false;
  bool known = false;
  for (auto& n : pt.names)
    if (n.first == id) {
      n.second = name;
      known = true;
    }
  if (!known) pt.names.push_back(std::make_pair(id, name));
  const auto rename = [&](Roster& roster) {
    for (RosterUnit& r : roster)
      if (r.id == id) r.name = name;
  };
  rename(pt.roster);
  for (ParkedParty& p : pt.parked) rename(p.roster);
  return true;
}

std::string characterName(const Database& db, const Playthrough& pt, const std::string& id) {
  if (const std::string* typed = typedName(pt, id)) return *typed;
  if (const RosterUnit* r = pt.findUnit(id)) return r->name;
  for (const ParkedParty& p : pt.parked)
    for (const RosterUnit& r : p.roster)
      if (r.id == id) return r.name;
  const int ch = db.findCharacter(id);
  return ch >= 0 ? db.characters[(size_t)ch].name : id;
}

namespace savejson {

void writePlaythroughNames(const Playthrough& pt, Json& out) {
  if (pt.names.empty()) return;  // every campaign that never asks saves exactly as before
  Json names = Json::makeObject();
  for (const auto& n : pt.names) names.set(n.first, Json::makeString(n.second));
  out.set("names", names);
}

bool readPlaythroughNames(Reader& rd, const Json& saved, Playthrough& pt) {
  if (!saved.has("names")) return true;
  const Json* names;
  if (!rd.object(saved, "names", "playthrough", names)) return false;
  for (const Json::Member& m : names->members()) {
    const std::string where = "playthrough.names." + m.first;
    if (!isStoryId(m.first)) return rd.fail(where, "not a character id (letters, digits, \"_\" and \"-\")");
    if (!m.second.isString() || !validEnteredName(m.second.asString()))
      return rd.fail(where, "must be a name of 1 to " + std::to_string(kMaxNameLetters) + " letters without \"{\" or \"}\"");
    pt.names.push_back(std::make_pair(m.first, m.second.asString()));
  }
  return true;
}

}  // namespace savejson
}  // namespace ote
