// The speed turn order in saves (docs/SAVES.md, docs/TURN_ORDER.md): every unit's "ct" (its charge) and "turnActed", and the battle's "clock":
// the unit whose turn it is, the tick of the round, the turns begun and the charged actions on their way. All written only in a speed game, and
// "ct" / "turnActed" only when not 0 / false. A save that has them in a game that is not a speed game, or that does not fit its units, is refused.
#include "core/battle/save_srpg.h"

namespace ote {
namespace savejson {

namespace {
constexpr int kMaxCharge = 100000000;
}

void writeUnitClock(const Battle& b, const Unit& u, Json& j) {
  if (!b.speedGame()) return;
  if (u.ct != 0) j.set("ct", Json::makeNumber(u.ct));
  if (u.turnActed) j.set("turnActed", Json::makeBool(true));
}

bool readUnitClock(Reader& rd, const Battle& b, const Json& j, const std::string& where, Unit& u) {
  u.ct = 0;
  u.turnActed = false;
  if (!j.has("ct") && !j.has("turnActed")) return true;
  if (!b.speedGame()) return rd.fail(where + (j.has("ct") ? ".ct" : ".turnActed"), "this game has no speed turn order");
  if (j.has("ct") && !rd.integer(j, "ct", where, -kMaxCharge, kMaxCharge, u.ct)) return false;
  return !j.has("turnActed") || rd.boolean(j, "turnActed", where, u.turnActed);
}

void writeBattleClock(const Battle& b, Json& out) {
  if (!b.speedGame()) return;
  Json c = Json::makeObject();
  c.set("activeUnit", Json::makeNumber(b.over ? -1 : b.activeUnit));  // (a finished battle has nobody to play)
  c.set("tick", Json::makeNumber(b.clockTick));
  c.set("turn", Json::makeNumber(b.turnNo));
  c.set("serial", Json::makeNumber(b.chargeSerial));
  Json list = Json::makeArray();
  for (const PendingAction& p : b.pending) {
    Json e = Json::makeObject();
    e.set("unit", Json::makeNumber(p.unit));
    e.set("kind", Json::makeString(p.weapon ? "weapon" : "ability"));
    e.set("id", Json::makeString(p.weapon ? b.db->weapons[(size_t)p.index].id : b.db->abilities[(size_t)p.index].id));
    e.set("counter", Json::makeNumber(p.counter));
    e.set("follow", Json::makeBool(p.follow));
    e.set("target", Json::makeNumber(p.target));
    e.set("cell", Json::makeNumber(p.cell));
    if (p.slot >= 0) e.set("slot", Json::makeNumber(p.slot));
    e.set("serial", Json::makeNumber(p.serial));
    list.push(e);
  }
  c.set("pending", list);
  out.set("clock", c);
}

bool readBattleClock(Reader& rd, const Json& saved, Battle& b) {
  b.activeUnit = -1;
  b.clockTick = b.turnNo = b.chargeSerial = 0;
  b.pending.clear();
  const SpeedRules& sp = b.db->rules.speed;
  if (!saved.has("clock")) {
    if (b.speedGame()) return rd.fail("battle.clock", "missing (this game plays with the speed turn order)");
    return true;
  }
  if (!b.speedGame()) return rd.fail("battle.clock", "this game has no speed turn order");
  const Json* c;
  if (!rd.object(saved, "clock", "battle", c)) return false;
  const std::string at = "battle.clock";
  const int units = (int)b.units.size();
  if (!rd.integer(*c, "activeUnit", at, -1, units - 1, b.activeUnit) || !rd.integer(*c, "tick", at, 0, sp.roundTicks - 1, b.clockTick) ||
      !rd.integer(*c, "turn", at, 0, kMaxNumber, b.turnNo) || !rd.integer(*c, "serial", at, 0, kMaxNumber, b.chargeSerial))
    return false;
  if (b.activeUnit >= 0) {
    const Unit& u = b.units[(size_t)b.activeUnit];
    if (!u.alive) return rd.fail(at + ".activeUnit", "unit " + std::to_string(b.activeUnit) + " is dead");
    if (u.side != b.activeSide) return rd.fail(at + ".activeUnit", "unit " + std::to_string(b.activeUnit) + " is not on the active side");
  } else if (!b.over) {
    return rd.fail(at + ".activeUnit", "-1, but the battle is not over: somebody has the turn");
  }
  const Json* list;
  if (!rd.array(*c, "pending", at, -1, kMaxUnits, list)) return false;
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& e = (*list)[i];
    const std::string w = at + ".pending[" + std::to_string(i) + "]";
    if (!e.isObject()) return rd.fail(w, "not an object");
    PendingAction p;
    if (!rd.integer(e, "unit", w, 0, units - 1, p.unit)) return false;
    const Unit& cu = b.units[(size_t)p.unit];
    if (!cu.alive) return rd.fail(w + ".unit", "unit " + std::to_string(p.unit) + " is dead");
    for (const PendingAction& o : b.pending)
      if (o.unit == p.unit) return rd.fail(w + ".unit", "unit " + std::to_string(p.unit) + " has another charged action already");
    if (!e["kind"].isString() || !e["id"].isString()) return rd.fail(w, "\"kind\" and \"id\" must be text");
    const std::string kind = e["kind"].asString(), id = e["id"].asString();
    if (kind != "weapon" && kind != "ability") return rd.fail(w + ".kind", "must be \"weapon\" or \"ability\"");
    p.weapon = kind == "weapon";
    p.index = p.weapon ? b.db->findWeapon(id) : b.db->findAbility(id);
    if (p.index < 0) return rd.fail(w + ".id", "\"" + id + "\" is not a " + kind + " of this game");
    const ChargeDef& def = p.weapon ? b.db->weapons[(size_t)p.index].charge : b.db->abilities[(size_t)p.index].charge;
    if (!def.on()) return rd.fail(w + ".id", "\"" + id + "\" does not charge");
    p.speed = def.speed;
    if (!rd.integer(e, "counter", w, 0, sp.threshold - 1, p.counter) || !rd.boolean(e, "follow", w, p.follow) ||
        !rd.integer(e, "target", w, -1, units - 1, p.target) || !rd.integer(e, "cell", w, -1, (int)b.cells.size() - 1, p.cell) ||
        !rd.integer(e, "serial", w, 1, kMaxNumber, p.serial))
      return false;
    if (e.has("slot") && !rd.integer(e, "slot", w, 0, 1000, p.slot)) return false;
    if (p.follow && p.target < 0) return rd.fail(w + ".target", "a charged action that follows a unit names it");
    if (p.serial > b.chargeSerial) return rd.fail(w + ".serial", "is above battle.clock.serial");
    b.pending.push_back(p);
  }
  return true;
}

}  // namespace savejson
}  // namespace ote
