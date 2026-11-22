// A writable copy of tests/fixtures/jobs (jobs_fixture.h) with learning from equipment and abilities that strike added (docs/JOBS.md "Learning from
// equipment", docs/SRPG.md "Abilities that strike", "Unit resources"). Map "duel": Aldric (Squire) at 2,2 next to the dummy "foe" (hp 200, def 2) at 3,2,
// Cora (Mage) at 1,0, the plain squire "plain" at 1,4, the grunt "weak" (hp 12) at 5,3, the red shaman (a Mage) at 6,3.
//   items:  charm (accessory) teaches the skills brave (20 points, no job), guard (30, also Squire's entry) and focus (40, also Mage's);
//           tome (accessory) teaches the ability mend (15, also Mage's)
//   rules.abilityPoints: AP, 10 a won battle, 5 an action, 3 a defeat
//   weapons: fire_spell (might 8, hit "100", range 1..3), dud (might 1, hit "b.def * 10"), blaze (might 5, range 1..3, a burst area)
//   abilities on the squire type: fire (strike fire_spell, spend 4 MP), flame (strike blaze), patch (heal 50 %), salve (heal "a.str * 2"), both on itself
//   unitResources: mp "MP", max = str, starts full, +1 a turn
#pragma once
#include "jobs_fixture.h"

namespace teachtest {

using namespace ote;

struct Copy : jobtest::Copy {
  explicit Copy(const std::string& name) : jobtest::Copy(name) {
    put("data/items.json", R"({ "items": [
      { "id": "charm", "name": "Charm", "kind": "accessory",
        "teaches": [ { "skill": "brave", "points": 20 }, { "skill": "guard", "points": 30 }, { "skill": "focus", "points": 40 } ] },
      { "id": "tome", "name": "Tome", "kind": "accessory", "teaches": [ { "ability": "mend", "points": 15 } ] } ] })");
    swap("data/skills.json", R"({ "skills": [)", R"({ "skills": [
      { "id": "brave", "name": "Brave", "description": "Strength +1.", "blocks": [ { "when": "always", "do": [ { "stats": { "str": 1 } } ] } ] },)");
    swap("game.json", R"("changeIn": "camp+prep" } },)", R"("changeIn": "camp+prep" },
             "abilityPoints": { "name": "AP", "perBattle": 10, "perAction": 5, "perDefeat": 3 } },
  "unitResources": [ { "id": "mp", "name": "MP", "max": "str", "regen": { "perTurn": 1 } } ],)");
    swap("data/weapons.json", R"({ "weapons": [)", R"({ "weapons": [
      { "id": "fire_spell", "name": "Fire", "range": [1, 3], "hit": "100", "props": { "might": 8 } },
      { "id": "dud", "name": "Dud", "hit": "b.def * 10", "props": { "might": 1 } },
      { "id": "blaze", "name": "Blaze", "range": [1, 3], "area": { "shape": "burst" }, "props": { "might": 5 } },)");
    swap("data/abilities.json", R"({ "abilities": [)", R"({ "abilities": [
      { "id": "fire", "name": "Fire", "effect": { "strike": "fire_spell" }, "cost": "action", "spend": { "mp": 4 } },
      { "id": "flame", "name": "Flame", "effect": { "strike": "blaze" }, "cost": "action" },
      { "id": "poke", "name": "Poke", "effect": { "strike": "dud" }, "cost": "action" },
      { "id": "patch", "name": "Patch", "target": "self", "effect": { "heal": { "percent": 50 } }, "cost": "action" },
      { "id": "salve", "name": "Salve", "target": "self", "effect": { "heal": "a.str * 2" }, "cost": "action" },)");
    swap("data/units.json", R"("weapons": ["sword"] },
  { "id": "knight")", R"("weapons": ["sword"], "abilities": ["fire", "flame", "poke", "patch", "salve"] },
  { "id": "knight")");
  }
};

// The duel map of a Copy, started.
struct Duel {
  Copy c;
  skilltest::World w;
  explicit Duel(const char* name, bool start = true) : c(name), w("duel", c.dir, start) {}
  Battle& b() { return w.b(); }
  const Database& db() { return w.db(); }
  Unit& unit(const char* id) { return w.unit(id); }
  int ability(const char* id) { return w.db().findAbility(id); }
  int teach(const char* id) {
    for (size_t i = 0; i < w.db().teachEntries.size(); ++i)
      if (w.db().teachEntries[i].refId == id) return (int)i;
    return -1;
  }
  int entry(const char* id) {
    for (const JobLearnDef& e : w.db().jobEntries)
      if (e.refId == id) return e.flat;
    return -1;
  }
  int job(const char* id) { return w.db().findJob(id); }
  void give(const char* unitId, const char* item) {
    ItemStack s;
    s.item = w.db().findItem(item);
    unit(unitId).items.push_back(s);
  }
  std::vector<Event> of(EventType t) const {
    std::vector<Event> out;
    for (const Event& e : w.events)
      if (e.type == t) out.push_back(e);
    return out;
  }
  bool strike(const char* who, const char* target) { return w.apply(Action::attack(w.id(who), w.id(target), w.db().findWeapon("sword"))); }
};

// A loader refusal of a Copy changed by `edit`: the error text ("" = it loaded).
template <typename F>
std::string refusal(const char* name, F edit) {
  Copy c(name);
  edit(c);
  Database db;
  std::string err;
  return loadGame(c.dir, db, err) ? "" : err;
}

}  // namespace teachtest
