// The skills of one strike (docs/SRPG.md "Combat pipeline" step 2 and "Forecast"): the striker's "strike" blocks and
// the struck unit's "struck" blocks, as certain effects (no chance), as rolled activations, or as forecast notes.
#include <algorithm>

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
      case K::Recoil: m.recoils.push_back(&e); break;
      default: break;  // the loader keeps other effects out of these blocks
    }
  }
}

bool hasRecoil(const SkillBlock& b) {
  for (const SkillEffect& e : b.effects)
    if (e.kind == SkillEffect::Kind::Recoil) return true;
  return false;
}

}  // namespace

void Battle::gatherStrike(const CombatSetup& setup, bool counter, int strikeNo, Rng* rng, StrikeMods& out,
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
    ctx.strikeNo = strikeNo;  // the striker's strike number, for the struck unit's blocks too
    for (int s : list) {
      for (const SkillBlock& b : db->skills[(size_t)s].blocks) {
        if (b.when != when) continue;
        // A block that asks about marks depends on a roll of another block, so no forecast lists it (the other block's chance is the note).
        if (!rng && db->marksUsed && blockReadsMarks(b)) continue;
        if (!skillBlockHolds(b, ctx)) continue;
        if (b.hasChance) {
          const int chance = skillChance(b, ctx);
          if (notes) {
            if (chance > 0) notes->push_back(ForecastNote{owner, s, chance, hasRecoil(b)});
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
          // A list of notes only wants the activations, and a recoil that is certain: the forecast says the weapon bites (100 %).
          if (hasRecoil(b) && side == 0) notes->push_back(ForecastNote{owner, s, 100, true});
          continue;
        }
        numbers.clear();
        for (const SkillEffect& e : b.effects) numbers.push_back(skillEffectNumber(e, ctx, scratch));
        applyStrikeEffects(b, numbers, out, side == 0);
        if (rng && db->marksUsed)  // a real fight: the owner now has the marks this block gives (blocks after it see them at once)
          for (const SkillEffect& e : b.effects)
            if (e.kind == SkillEffect::Kind::Mark) setMark(owner, e.names[0]);
      }
    }
  }
}

void Battle::strikeActivations(const CombatSetup& setup, bool counter, int strikeNo, StrikeMods& out, std::vector<Event>& strikeEvents,
                               std::vector<Event>& struckEvents) {
  gatherStrike(setup, counter, strikeNo, &rng, out, &strikeEvents, &struckEvents, nullptr);
}

void Battle::strikePassives(const CombatSetup& setup, bool counter, int strikeNo, StrikeMods& out) const {
  gatherStrike(setup, counter, strikeNo, nullptr, out, nullptr, nullptr, nullptr);
}

// Roll order of a real fight: the attacker's strike, then (if the defender answers) the defender's. A game whose blocks ask which strike
// it is (`strikeNo`) lists, after the first strike's activations, the ones only a later strike of the same fighter would roll (once each).
void Battle::forecastNotes(const CombatSetup& setup, const Forecast& f, std::vector<ForecastNote>& out) const {
  out.clear();
  StrikeMods unused;
  for (int side = 0; side < 2; ++side) {
    const bool counter = side == 1;
    if (counter && !setup.counters) break;
    gatherStrike(setup, counter, 1, nullptr, unused, nullptr, nullptr, &out);
    if (!db->strikeNoUsed) continue;
    const StrikeForecast& sf = counter ? f.counter : f.attack;
    const int total = std::max(1, sf.strikes * std::max(1, sf.multi));
    for (int no = 2; no <= total; ++no) {
      std::vector<ForecastNote> later;
      gatherStrike(setup, counter, no, nullptr, unused, nullptr, nullptr, &later);
      for (const ForecastNote& n : later) {
        bool known = false;
        for (const ForecastNote& o : out) known = known || (o.unit == n.unit && o.skill == n.skill);
        if (!known) out.push_back(n);
      }
    }
  }
}

}  // namespace ote
