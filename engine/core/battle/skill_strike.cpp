// The skills of one strike (docs/SRPG.md "Combat pipeline" step 2 and "Forecast"): the striker's "strike" blocks and
// the struck unit's "struck" blocks, as certain effects (no chance), as rolled activations, or as forecast notes.
#include "core/battle/combat_mods.h"

namespace ote {

namespace {

// The effects a "strike" block (the striker's) or a "struck" block (the struck unit's) has on this one strike.
void applyStrikeEffects(const SkillBlock& b, StrikeMods& m) {
  using K = SkillEffect::Kind;
  for (const SkillEffect& e : b.effects) {
    switch (e.kind) {
      case K::Hit: m.hit += e.value; break;
      case K::Crit: m.crit += e.value; break;
      case K::Damage: m.damage += e.value; break;
      case K::DamagePercent: m.damagePercent.push_back(e.value); break;
      case K::Drain: m.drain += e.value; break;
      case K::StrikeAgain: m.strikeAgain = true; break;
      case K::Avoid: m.avoid += e.value; break;
      case K::CritAvoid: m.critAvoid += e.value; break;
      case K::DamageTaken: m.damageTaken += e.value; break;
      case K::DamageTakenPercent: m.damageTakenPercent.push_back(e.value); break;
      case K::Survive: m.survive = true; break;
      default: break;  // the loader keeps other effects out of these blocks
    }
  }
}

}  // namespace

void Battle::gatherStrike(const CombatSetup& setup, bool counter, Rng* rng, StrikeMods& out,
                          std::vector<Event>* strikeEvents, std::vector<Event>* struckEvents,
                          std::vector<ForecastNote>* notes) const {
  if (db->skills.empty()) return;
  const int striker = counter ? setup.defender : setup.attacker;
  const int struck = counter ? setup.attacker : setup.defender;
  std::vector<int> list;
  for (int side = 0; side < 2; ++side) {  // roll order: the striker's blocks, then the struck unit's
    const int owner = side == 0 ? striker : struck;
    const SkillWhen when = side == 0 ? SkillWhen::Strike : SkillWhen::Struck;
    skillCombatList(setup, owner, list);
    const SkillCtx ctx = skillCombatCtx(setup, owner);
    for (int s : list) {
      for (const SkillBlock& b : db->skills[(size_t)s].blocks) {
        if (b.when != when || !skillBlockHolds(b, ctx)) continue;
        if (b.hasChance) {
          const int chance = skillChance(b, ctx);
          if (notes) {
            if (chance > 0) notes->push_back(ForecastNote{owner, s, chance});
            continue;
          }
          if (!rng || !rollActivation(*rng, chance)) continue;
          Event e;
          e.type = EventType::SkillActivated;
          e.a = owner;
          e.b = s;
          e.c = ctx.foe;
          (side == 0 ? strikeEvents : struckEvents)->push_back(e);
        } else if (notes) {
          continue;  // a list of notes only wants the activations
        }
        applyStrikeEffects(b, out);
      }
    }
  }
}

void Battle::strikeActivations(const CombatSetup& setup, bool counter, StrikeMods& out, std::vector<Event>& strikeEvents,
                               std::vector<Event>& struckEvents) {
  gatherStrike(setup, counter, &rng, out, &strikeEvents, &struckEvents, nullptr);
}

void Battle::strikePassives(const CombatSetup& setup, bool counter, StrikeMods& out) const {
  gatherStrike(setup, counter, nullptr, out, nullptr, nullptr, nullptr);
}

// Roll order of a real fight: the attacker's strike, then (if the defender answers) the defender's.
void Battle::forecastNotes(const CombatSetup& setup, std::vector<ForecastNote>& out) const {
  out.clear();
  StrikeMods unused;
  gatherStrike(setup, false, nullptr, unused, nullptr, nullptr, &out);
  if (setup.counters) gatherStrike(setup, true, nullptr, unused, nullptr, nullptr, &out);
}

}  // namespace ote
