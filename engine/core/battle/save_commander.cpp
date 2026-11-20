#include "core/battle/save_commander.h"

namespace ote {
namespace savejson {

namespace {
Json whole(int v) { return Json::makeNumber((double)v); }
}  // namespace

void writeSideCommander(const Database& db, const SideState& s, Json& side) {
  if (s.commander < 0 || (size_t)s.commander >= db.commanders.size()) return;
  Json c = Json::makeObject();
  c.set("id", Json::makeString(db.commanders[(size_t)s.commander].id));
  c.set("meter", whole(s.meter));
  if (s.power >= 0) {
    c.set("power", whole(s.power));
    c.set("left", whole(s.powerLeft));
  }
  Json uses = Json::makeArray();
  for (int u : s.powerUses) uses.push(whole(u));
  c.set("uses", uses);
  side.set("commander", c);
}

bool readSideCommander(Reader& rd, const Database& db, const Json& side, const std::string& where, SideState& s) {
  const bool had = s.commander >= 0;  // the side as the map made it
  s.commander = -1;
  s.meter = 0;
  s.power = -1;
  s.powerLeft = 0;
  s.powerUses.clear();
  const std::string at = where + ".commander";
  if (!side.has("commander")) {
    if (had) return rd.fail(at, "missing (the map gives this side a commander)");
    return true;
  }
  if (db.commanders.empty()) return rd.fail(at, "the game has no commanders");
  const Json* c;
  if (!rd.object(side, "commander", where, c)) return false;
  if (!(*c)["id"].isString()) return rd.fail(at + ".id", "missing or not a commander id");
  const std::string id = (*c)["id"].asString();
  const int ci = db.findCommander(id);
  if (ci < 0) return rd.fail(at + ".id", "names unknown commander \"" + id + "\" (data/commanders.json)");
  const CommanderDef& d = db.commanders[(size_t)ci];
  int meter = 0;
  if (!rd.integer(*c, "meter", at, 0, d.meterMax, meter)) return false;
  int power = -1, left = 0;
  if (c->has("power")) {
    if (!rd.integer(*c, "power", at, 0, (int)d.powers.size() - 1, power)) return false;
    const CommanderPower& p = d.powers[(size_t)power];
    if (p.modifiers.empty()) return rd.fail(at + ".power", "\"" + p.id + "\" has no timed skills, it cannot be active");
    if (!rd.integer(*c, "left", at, 1, p.turns, left)) return false;
  } else if (c->has("left")) {
    return rd.fail(at + ".left", "there is no active power");
  }
  const Json* uses;
  if (!rd.array(*c, "uses", at, (int)d.powers.size(), 0, uses)) return false;
  std::vector<int> counts;
  for (size_t i = 0; i < uses->size(); ++i) {
    const Json& v = (*uses)[i];
    if (!v.isNumber() || v.asNumber() != std::floor(v.asNumber()) || v.asNumber() < 0 || v.asNumber() > 1000000)
      return rd.fail(at + ".uses[" + std::to_string(i) + "]", "not a whole number from 0 to 1000000");
    counts.push_back((int)v.asNumber());
  }
  if (power >= 0 && counts[(size_t)power] < 1) return rd.fail(at + ".uses", "the active power was never used");
  // Set up exactly as a live battle has it: passive skills after the faction's, the active power's last.
  s.commander = ci;
  s.meter = meter;
  s.power = power;
  s.powerLeft = left;
  s.powerUses = counts;
  size_t at0 = 0;
  while (at0 < s.modifiers.size() && s.modifiers[at0].faction >= 0) ++at0;
  std::vector<SideModifier> mods;
  commanderSideModifiers(db, ci, mods);
  s.modifiers.insert(s.modifiers.begin() + (long)at0, mods.begin(), mods.end());
  if (power >= 0) {
    commanderPowerModifiers(db, ci, power, mods);
    s.modifiers.insert(s.modifiers.end(), mods.begin(), mods.end());
  }
  return true;
}

}  // namespace savejson
}  // namespace ote
