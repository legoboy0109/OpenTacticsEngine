// Commanders (docs/COMMANDERS.md): the passive skills a side's commander gives, the power meter and what fills it, and a power's timed phase.
// Everything here is a no-op for a side without a commander (a game without data/commanders.json has none), so such a game plays, saves
// and simulates exactly as before. The instant effects and the UsePower action are in commander_power.cpp.
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

const CommanderDef* Battle::commanderOf(int side) const {
  if (side < 0 || side >= (int)sides.size()) return nullptr;
  const int c = sides[(size_t)side].commander;
  return c >= 0 && (size_t)c < db->commanders.size() ? &db->commanders[(size_t)c] : nullptr;
}

int Battle::meterOf(int side) const { return commanderOf(side) ? sides[(size_t)side].meter : 0; }
int Battle::meterMax(int side) const { const CommanderDef* d = commanderOf(side); return d ? d->meterMax : 0; }
int Battle::activePower(int side) const { return commanderOf(side) ? sides[(size_t)side].power : -1; }
int Battle::powerTurnsLeft(int side) const { return activePower(side) >= 0 ? sides[(size_t)side].powerLeft : 0; }

int Battle::powerUses(int side, int power) const {
  const CommanderDef* d = commanderOf(side);
  if (!d || power < 0 || (size_t)power >= d->powers.size()) return 0;
  const std::vector<int>& uses = sides[(size_t)side].powerUses;
  return (size_t)power < uses.size() ? uses[(size_t)power] : 0;
}

int Battle::powerCost(int side, int power) const {
  const CommanderDef* d = commanderOf(side);
  if (!d || power < 0 || (size_t)power >= d->powers.size()) return 0;
  const CommanderPower& p = d->powers[(size_t)power];
  // Each earlier use adds growthPercent of the base cost (whole points, rounded down); a power can never cost more than the meter holds.
  const long long grown = (long long)p.cost + (long long)p.cost * p.growthPercent * powerUses(side, power) / 100;
  return (int)std::min<long long>(grown, d->meterMax);
}

int Battle::powerShortBy(int side, int power) const {
  return std::max(0, powerCost(side, power) - meterOf(side));
}

bool Battle::canUsePower(int side, int power, std::string* why) const {
  auto no = [&](const std::string& reason) {
    if (why) *why = reason;
    return false;
  };
  const CommanderDef* d = commanderOf(side);
  if (over) return no("the battle is over");
  if (!d) return no("this side has no commander");
  if (power < 0 || (size_t)power >= d->powers.size()) return no("that commander has no such power");
  if (side != activeSide || sides[(size_t)side].defeated) return no("it is not that side's turn");
  // A speed game has no "side's turn", only a unit's: a power may be used while the active unit is the side's own.
  if (speedGame() && !(activeUnit >= 0 && (size_t)activeUnit < units.size() && units[(size_t)activeUnit].side == side))
    return no("it is not that side's unit's turn");
  if (sides[(size_t)side].power >= 0) return no("a power is active already");
  if (const int lack = powerShortBy(side, power); lack > 0) return no("needs " + std::to_string(lack) + " more");
  return true;
}

int Battle::defaultCommanderChoice(const Database& db, uint32_t seed, int side, bool ai, int faction) {
  if (db.commanders.empty()) return -1;
  std::vector<int> allowed;
  if (faction >= 0 && (size_t)faction < db.factions.size() && db.factions[(size_t)faction].limitCommanders)
    allowed = db.factions[(size_t)faction].commanders;
  else
    for (size_t i = 0; i < db.commanders.size(); ++i) allowed.push_back((int)i);
  if (allowed.empty()) return -1;
  if (!ai) return allowed[0];
  // The same mixing as the faction pick but other constants: a side's faction and commander must not move together. Not the battle's own
  // random numbers: a "choose" must not shift the stream.
  uint64_t x = (uint64_t)seed * 0xD6E8FEB86659FD93ull + (uint64_t)(side + 1) * 0xA0761D6478BD642Full;
  x ^= x >> 32;
  x *= 0xE7037ED1A0B428DBull;
  x ^= x >> 29;
  x *= 0x8EBC6AF09C88C6E3ull;
  x ^= x >> 32;
  return allowed[(size_t)(x % allowed.size())];
}

void commanderSideModifiers(const Database& db, int commander, std::vector<SideModifier>& out) {
  out.clear();
  if (commander < 0 || (size_t)commander >= db.commanders.size()) return;
  const std::vector<SkillGrant>& list = db.commanders[(size_t)commander].passive;
  for (size_t g = 0; g < list.size(); ++g) {
    SideModifier m;
    m.skill = list[g].skill;
    m.filterType = list[g].filterType;
    m.filterTag = list[g].filterTag;
    m.commander = commander;
    m.grant = (int)g;
    out.push_back(m);
  }
}

void commanderPowerModifiers(const Database& db, int commander, int power, std::vector<SideModifier>& out) {
  out.clear();
  if (commander < 0 || (size_t)commander >= db.commanders.size()) return;
  const CommanderDef& d = db.commanders[(size_t)commander];
  if (power < 0 || (size_t)power >= d.powers.size()) return;
  const std::vector<SkillGrant>& list = d.powers[(size_t)power].modifiers;
  for (size_t g = 0; g < list.size(); ++g) {
    SideModifier m;
    m.skill = list[g].skill;
    m.filterType = list[g].filterType;
    m.filterTag = list[g].filterTag;
    m.commander = commander;
    m.power = power;
    m.grant = (int)g;
    out.push_back(m);
  }
}

void Battle::takeCommander(int side, int c) {
  if (side < 0 || side >= (int)sides.size() || c < 0 || (size_t)c >= db->commanders.size()) return;
  SideState& s = sides[(size_t)side];
  s.commander = c;
  s.meter = 0;
  s.power = -1;
  s.powerLeft = 0;
  s.powerUses.assign(db->commanders[(size_t)c].powers.size(), 0);
  std::vector<SideModifier> mods;
  commanderSideModifiers(*db, c, mods);
  for (const SideModifier& m : mods) addSideModifier(side, m);
}

void Battle::fillMeter(int side, long long points) {
  const CommanderDef* d = commanderOf(side);
  if (!d || points <= 0) return;
  SideState& s = sides[(size_t)side];
  if (s.power >= 0) return;  // the meter rests while the side's power is active
  s.meter = (int)std::min<long long>(d->meterMax, (long long)s.meter + points);
}

void Battle::commanderStrike(int attackerSide, int defenderSide, int damage) {
  if (damage <= 0) return;
  if (const CommanderDef* a = commanderOf(attackerSide)) fillMeter(attackerSide, (long long)damage * a->perDamageDealt);
  if (const CommanderDef* d = commanderOf(defenderSide)) fillMeter(defenderSide, (long long)damage * d->perDamageTaken);
}

void Battle::endPower(int side, std::vector<Event>& events) {
  SideState& s = sides[(size_t)side];
  const int power = s.power;
  if (power < 0) return;
  s.modifiers.erase(std::remove_if(s.modifiers.begin(), s.modifiers.end(), [](const SideModifier& m) { return m.commander >= 0 && m.power >= 0; }),
                    s.modifiers.end());
  s.power = -1;
  s.powerLeft = 0;
  for (Unit& u : units)  // a skill that raised the maximum is gone: hit points stay within it, as for research
    if (exists(u) && u.side == side) clampHp(u);
  Event e;
  e.type = EventType::PowerEnded;
  e.a = side;
  e.b = s.commander;
  e.c = power;
  events.push_back(e);
}

void Battle::commanderTurnStart(std::vector<Event>& events) {
  if (!commanderOf(activeSide)) return;
  SideState& s = sides[(size_t)activeSide];
  if (s.power >= 0 && --s.powerLeft <= 0) endPower(activeSide, events);
  fillMeter(activeSide, commanderOf(activeSide)->perTurn);  // (nothing while a power is still active)
}

}  // namespace ote
