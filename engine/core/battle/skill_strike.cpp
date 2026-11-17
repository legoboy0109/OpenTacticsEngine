// The skills of one strike (docs/SRPG.md "Combat pipeline" step 2 and "Forecast"): the striker's "strike" blocks and
// the struck unit's "struck" blocks, as certain effects (no chance), as rolled activations, or as forecast notes.
#include "core/battle/combat_mods.h"

namespace ote {

namespace {

// The effects a "strike" block (the striker's) or a "struck" block (the struck unit's) has on this one strike.
// `numbers[i]` is effect i's number (a formula amount already worked out: `a` = the owner, `b` = the foe).
void applyStrikeEffects(const SkillBlock& b, const std::vector<int>& numbers, StrikeMods& m, bool strikersBlock) {
  using K = SkillEffect::Kind;
  for (size_t i = 0; i < b.effects.size(); ++i) {
    const SkillEffect& e = b.effects[i];
    const int v = numbers[i];
    switch (e.kind) {
      case K::Hit: m.hit += v; break;
      case K::Crit: m.crit += v; break;
      case K::Damage: m.damage += v; break;
      case K::DamagePercent: m.damagePercent.push_back(v); break;
      case K::Drain: m.drain += e.value; break;
      case K::StrikeAgain: m.strikeAgain = true; break;
      case K::Avoid: m.avoid += v; break;
      case K::CritAvoid: m.critAvoid += v; break;
      case K::DamageTaken: m.damageTaken += v; break;
      case K::DamageTakenPercent: m.damageTakenPercent.push_back(v); break;
      case K::Survive: m.survive = true; break;
      case K::Inflict: (strikersBlock ? m.inflictOnStruck : m.inflictOnStriker).push_back(&e); break;
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
  std::vector<int> list, numbers, scratch;
  for (int side = 0; side < 2; ++side) {  // roll order: the striker's blocks, then the struck unit's
    const int owner = side == 0 ? striker : struck;
    const SkillWhen when = side == 0 ? SkillWhen::Strike : SkillWhen::Struck;
    skillCombatList(setup, owner, list);
    SkillCtx ctx = skillCombatCtx(setup, owner);
    ctx.counters = setup.counters ? 1 : 0;  // known here, unlike while the combat's modifiers are being collected
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
        numbers.clear();
        for (const SkillEffect& e : b.effects) numbers.push_back(skillEffectNumber(e, ctx, scratch));
        applyStrikeEffects(b, numbers, out, side == 0);
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
