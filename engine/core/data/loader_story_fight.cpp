// Loader piece of docs/EVENTS.md "A staged fight":
//   { "fight": { "attacker": "kay", "defender": "brigand", "strikes": [ { "by": "attacker", "result": "hit", "damage": 3 }, ... ],
//                "lethal": false, "xp": true, "wear": true, "weapon": { "attacker": "iron_sword", "defender": "iron_axe" } } }
#include "core/data/loader_events.h"
#include "core/data/loader_util.h"

namespace ote {

constexpr int kMaxStagedStrikes = 24;  // generous: no weapon strikes more than a handful of times

bool StoryLoader::fightCommand(const Json& e, const std::string& at, Command& out) {
  out.kind = Command::Kind::Fight;
  if (!onlyKeys(e, {"fight"}, at)) return false;
  const Json& f = e["fight"];
  const std::string in = "\"fight\": ";
  if (!f.isObject()) return fail(at, "\"fight\" must be { \"attacker\": unit id, \"defender\": unit id, ... }");
  if (!onlyKeys(f, {"attacker", "defender", "strikes", "weapon", "lethal", "xp", "wear"}, at + ": \"fight\"")) return false;
  StoryFight& s = out.fight;
  if (!f.has("attacker")) return fail(at, "missing required \"attacker\" (the unit that starts the fight)");
  if (!f.has("defender")) return fail(at, "missing required \"defender\" (the unit that is attacked)");
  if (!unitId(f["attacker"], at, "\"attacker\"", s.attacker) || !unitId(f["defender"], at, "\"defender\"", s.defender)) return false;
  if (s.attacker == s.defender) return fail(at, in + "a unit cannot fight itself");
  if (!c_.boolean(f, "lethal", at, s.lethal) || !c_.boolean(f, "xp", at, s.xp) || !c_.boolean(f, "wear", at, s.wear)) return false;
  if (f.has("weapon")) {  // { "attacker": id, "defender": id }: either may be left out (that side uses its equipped weapon)
    const Json& w = f["weapon"];
    if (!w.isObject() || w.size() == 0) return fail(at, in + "\"weapon\" must be { \"attacker\": weapon id, \"defender\": weapon id } (either may be left out)");
    if (!onlyKeys(w, {"attacker", "defender"}, at + ": \"weapon\"")) return false;
    for (const char* who : {"attacker", "defender"}) {
      if (!w.has(who)) continue;
      if (!w[who].isString() || db_.findWeapon(w[who].asString()) < 0)
        return fail(at, std::string(in) + "\"weapon\".\"" + who + "\" names unknown weapon \"" + (w[who].isString() ? w[who].asString() : std::string("?")) + "\" (data/weapons.json)");
      (std::string(who) == "attacker" ? s.weaponA : s.weaponD) = db_.findWeapon(w[who].asString());
    }
  }
  if (!f.has("strikes")) return true;  // an ordinary fight with real rolls
  const Json& list = f["strikes"];
  if (!list.isArray() || list.size() == 0 || (int)list.size() > kMaxStagedStrikes)
    return fail(at, in + "\"strikes\" must be a list of 1 to " + std::to_string(kMaxStagedStrikes) + " strikes such as { \"by\": \"attacker\", \"result\": \"hit\" }");
  s.scripted = true;
  for (size_t i = 0; i < list.size(); ++i) {
    const Json& st = list[i];
    const std::string where = at + ": strike " + std::to_string(i + 1);
    if (!st.isObject()) return fail(where, "must be { \"by\": \"attacker\" or \"defender\", \"result\": \"hit\", \"miss\" or \"crit\", \"damage\": number }");
    if (!onlyKeys(st, {"by", "result", "damage"}, where)) return false;
    StoryFightStrike k;
    std::string by, result;
    if (!c_.str(st, "by", where, by, true) || !c_.str(st, "result", where, result, true)) return false;
    if (by != "attacker" && by != "defender") return fail(where, "\"by\" must be \"attacker\" or \"defender\"");
    k.byAttacker = by == "attacker";
    if (result == "hit") k.result = 0;
    else if (result == "miss") k.result = 1;
    else if (result == "crit") k.result = 2;
    else return fail(where, "\"result\" must be \"hit\", \"miss\" or \"crit\"");
    if (st.has("damage")) {
      if (!LoadCtx::isWhole(st["damage"]) || st["damage"].asInt() < 0 || st["damage"].asInt() > 9999)
        return fail(where, "\"damage\" must be a whole number from 0 to 9999");
      k.damage = st["damage"].asInt();
      if (k.result == 1 && k.damage != 0) return fail(where, "a miss does no damage: leave \"damage\" out or write 0");
    }
    s.strikes.push_back(k);
  }
  return true;
}

}  // namespace ote
