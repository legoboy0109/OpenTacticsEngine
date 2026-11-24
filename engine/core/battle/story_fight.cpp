// The `fight` story command (docs/EVENTS.md "A staged fight"): two named map units fight, either with every strike written down (no
// roll of the battle's random numbers is made) or with real rolls. The strikes are the same Attack events a real fight makes, so
// the player shows the battle scene (or the on-map blows) exactly as for a fight a player started.
//
// What a staged fight leaves out on purpose, because it is a scene and not a move of the game: the "fight" story trigger and the
// characters' battle quotes before it, the weapon ranks, supports' "fought together" and "afterCombat" skill blocks after it, and
// (with `strikes`) every skill block that fires on a strike. The attacker's turn is not touched.
#include <algorithm>
#include <cmath>

#include "core/base/log.h"
#include "core/battle/battle.h"
#include "core/battle/combat_mods.h"

namespace ote {

namespace {

// Restores the story fight's switches however the command ends (a defeat in the middle may run another staged fight).
struct FightSwitches {
  bool& noWear;
  bool& spare;
  bool wasNoWear, wasSpare;
  FightSwitches(bool& w, bool& s, bool wear, bool lethal) : noWear(w), spare(s), wasNoWear(w), wasSpare(s) {
    noWear = !wear;
    spare = !lethal;
  }
  ~FightSwitches() {
    noWear = wasNoWear;
    spare = wasSpare;
  }
};

}  // namespace

void Battle::stagedStrike(const CombatSetup& setup, bool counter, const StoryFightStrike& k, bool lethal, bool wear, bool& ammoSpent,
                          std::vector<Event>& events) {
  const int atk = counter ? setup.defender : setup.attacker;
  const int def = counter ? setup.attacker : setup.defender;
  const int weapon = counter ? setup.weaponD : setup.weaponA;
  if (weapon < 0) {
    OTE_INFO("story: fight: '%s' has no weapon to strike with; that strike is skipped", units[(size_t)atk].storyId.c_str());
    return;
  }
  const int strikeNo = ++fightStrikes_[counter ? 1 : 0];
  int damage = std::max(0, k.damage);
  if (k.result != 1 && k.damage < 0) {  // the normal computed damage of this strike, with its blocks that need no roll
    CombatMods modsA, modsD;
    combatMods(setup, modsA, modsD);
    StrikeForecast f;
    if (strikeForecast(setup, counter, counter ? modsD : modsA, counter ? modsA : modsD, f)) {
      StrikeMods strike;
      strikePassives(setup, counter, strikeNo, strike);
      foldStrikeMods(strike, f);
      damage = f.damage;
      if (k.result == 2) damage = (int)std::floor(damage * db->rules.critMultiplier);
      damage = applyPercents(applyPercents(damage, strike.damagePercent), strike.damageTakenPercent);
    } else {
      damage = 0;  // the weapon cannot hurt that unit at all
    }
  }
  Event e;
  e.type = EventType::Attack;
  e.a = atk;
  e.b = def;
  e.c = weapon;
  e.d = 0;
  e.flags = counter ? kCounter : 0;
  if (k.result != 1) {
    Unit& target = units[(size_t)def];
    if (!lethal && damage >= target.hp) damage = std::max(0, target.hp - 1);
    damage = std::max(0, std::min(damage, target.hp));
    target.hp -= damage;
    if (damage > 0 && target.hp > 0) wakeUnit(def);
    e.d = damage;
    e.flags |= kHit;
    if (k.result == 2) e.flags |= kCrit;
    if (target.hp <= 0) e.flags |= kKill;
  }
  events.push_back(e);
  if (wear) {
    if (!ammoSpent) spendAmmo(atk, weapon);
    ammoSpent = true;
    const int slot = db->rules.items.carriedWeapons ? weaponSlot(units[(size_t)atk], weapon) : -1;
    if (slot >= 0) spendUse(atk, slot, events);
  }
  if (e.flags & kKill) {  // the usual defeat: drops to the victor, last words, the story's "defeated" events
    std::vector<ItemStack> drops;
    dropsOf(units[(size_t)def], drops);  // docs/ITEMS.md "Drops"
    if (!killUnit(def, events, atk)) return;  // caught by an "autoRevive" condition: no defeat
    for (const ItemStack& s : drops) giveTo(atk, s, kDropped, events);
    defeatQuote(def, events);
    unitDefeated(def, atk, events);  // may run story events: nothing after it may hold a reference into `units`
  }
}

void Battle::storyFight(const StoryFight& f, std::vector<Event>& events) {
  const int a = unitByStoryId(f.attacker), d = unitByStoryId(f.defender);
  auto onMap = [&](int u) { return u >= 0 && units[(size_t)u].alive && units[(size_t)u].cell != kNoCell; };
  if (over || !onMap(a) || !onMap(d)) {
    OTE_INFO("story: fight '%s' against '%s': a fighter is not on the map; no fight", f.attacker.c_str(), f.defender.c_str());
    return;
  }
  const Cell from = units[(size_t)a].cell;
  auto owns = [&](int unit, int weapon) {
    std::vector<int> owned;
    attackWeaponsAt(units[(size_t)unit], units[(size_t)unit].cell, owned);
    return std::find(owned.begin(), owned.end(), weapon) != owned.end();
  };
  const int weaponA = f.weaponA >= 0 ? f.weaponA : restingWeapon(units[(size_t)a]);
  if (weaponA < 0 || !owns(a, weaponA)) {
    OTE_INFO("story: fight: '%s' has no such weapon to attack with; no fight", f.attacker.c_str());
    return;
  }
  if (f.weaponD >= 0 && !owns(d, f.weaponD)) {
    OTE_INFO("story: fight: '%s' does not have that weapon; no fight", f.defender.c_str());
    return;
  }
  Forecast fc;
  CombatSetup setup;
  if (!forecastFight(a, d, weaponA, from, fc, setup, true)) {
    OTE_INFO("story: fight: '%s' cannot hurt '%s' with that weapon; no fight", f.attacker.c_str(), f.defender.c_str());
    return;
  }
  setup.mapStructure = -1;  // a staged fight uses carried weapons only
  if (f.weaponD >= 0) {     // the defender's weapon is named: it answers (range is not asked of a staged fight)
    setup.weaponD = f.weaponD;
    setup.counters = db->rules.counterAttacks;
    CombatMods mA, mD;
    combatMods(setup, mA, mD);
    setup.counters = setup.counters && !mA.noCounter;
    fc.counter = StrikeForecast();
    if (setup.counters && !strikeForecast(setup, true, mD, mA, fc.counter)) setup.counters = false;
    fc.defenderFirst = setup.counters && mD.strikeFirst && !mA.strikeFirst;
  }

  FightSwitches sw(fightNoWear_, fightSpare_, f.wear, f.lethal);
  const int hpA = units[(size_t)a].hp, hpD = units[(size_t)d].hp;
  fightStrikes_[0] = fightStrikes_[1] = 0;
  fightMarks_.clear();
  bool ammo[2] = {false, false};
  if (f.scripted) {
    for (const StoryFightStrike& k : f.strikes) {
      if (over || !units[(size_t)a].alive || !units[(size_t)d].alive) break;  // the fight ended early: the rest is not played
      stagedStrike(setup, !k.byAttacker, k, f.lethal, f.wear, ammo[k.byAttacker ? 0 : 1], events);
    }
  } else {
    std::vector<StrikeStep> order;
    strikeOrder(fc, order);
    for (const StrikeStep& step : order)
      for (int i = 0; i < step.count; ++i) {
        if (over || !units[(size_t)a].alive || !units[(size_t)d].alive) break;
        bool& spent = ammo[step.counter ? 1 : 0];
        if (f.wear && !spent) spendAmmo(step.counter ? d : a, step.counter ? setup.weaponD : setup.weaponA);
        spent = true;
        doStrike(setup, step.counter, false, events);
      }
  }
  if (f.xp) fightExperience(a, d, hpA, hpD, events);
  fightMarks_.clear();
}

}  // namespace ote
