#include "battle/commander_text.h"

#include "ui/text.h"

namespace ote {

namespace {

std::string skillList(const Database& db, const std::vector<SkillGrant>& list) {
  std::string out;
  for (const SkillGrant& g : list) {
    std::string name = db.skills[(size_t)g.skill].name;
    if (g.filterType >= 0) name += " (" + db.unitTypes[(size_t)g.filterType].name + ")";
    else if (!g.filterTag.empty()) name += " (" + g.filterTag + ")";
    out += (out.empty() ? "" : ", ") + name;
  }
  return out;
}

std::string effectText(const Database& db, const CommanderEffect& fx) {
  std::string who = fx.filterType >= 0 ? " (" + db.unitTypes[(size_t)fx.filterType].name + ")" : fx.filterTag.empty() ? "" : " (" + fx.filterTag + ")";
  switch (fx.kind) {
    case CommanderEffect::Kind::Heal: return "heals " + intStr(fx.amount) + who;
    case CommanderEffect::Kind::Damage: return "hurts enemies " + intStr(fx.amount) + who;
    case CommanderEffect::Kind::GiveResource: {
      std::string s;
      for (const Amount& a : fx.resources) s += (s.empty() ? "+" : ", +") + intStr(a.amount) + " " + db.resources[(size_t)a.resource].name;
      return s;
    }
    case CommanderEffect::Kind::Refresh: return "units act again" + who;
  }
  return "";
}

}  // namespace

std::string meterText(const Battle& b, int side) {
  return b.commanderOf(side) ? intStr(b.meterOf(side)) + "/" + intStr(b.meterMax(side)) : "";
}

std::string powerEntryLabel(const Battle& b, int side, int power) {
  const CommanderDef* d = b.commanderOf(side);
  return d && power >= 0 && (size_t)power < d->powers.size() ? CommanderWords::kPowerEntry + d->powers[(size_t)power].name : "";
}

std::string powerEntryExtra(const Battle& b, int side, int power, bool& enabled) {
  enabled = b.canUsePower(side, power);
  if (enabled) return intStr(b.powerCost(side, power));
  if (b.activePower(side) >= 0) return CommanderWords::kActive;
  const int lack = b.powerShortBy(side, power);
  return lack > 0 ? CommanderWords::kNeeds + intStr(lack) + CommanderWords::kMore : intStr(b.powerCost(side, power));
}

bool anyPowerReady(const Battle& b, int side) {
  const CommanderDef* d = b.commanderOf(side);
  for (size_t i = 0; d && i < d->powers.size(); ++i)
    if (b.canUsePower(side, (int)i)) return true;
  return false;
}

std::string powerSummary(const Database& db, const CommanderPower& p) {
  if (!p.description.empty()) return p.description;
  std::string out;
  if (!p.modifiers.empty()) out = skillList(db, p.modifiers) + " for " + intStr(p.turns) + (p.turns == 1 ? " turn" : " turns");
  for (const CommanderEffect& fx : p.effects) out += (out.empty() ? "" : "; ") + effectText(db, fx);
  return out;
}

std::vector<std::string> commanderLines(const Database& db, const CommanderDef& d) {
  std::vector<std::string> out;
  if (!d.passive.empty()) out.push_back(CommanderWords::kPassive + skillList(db, d.passive));
  for (const CommanderPower& p : d.powers) out.push_back(p.name + " (" + intStr(p.cost) + "): " + powerSummary(db, p));
  std::string rule;
  if (d.perDamageDealt > 0) rule += "+" + intStr(d.perDamageDealt) + " per damage dealt";
  if (d.perDamageTaken > 0) rule += (rule.empty() ? "+" : ", +") + intStr(d.perDamageTaken) + " per damage taken";
  if (d.perTurn > 0) rule += (rule.empty() ? "+" : ", +") + intStr(d.perTurn) + " per turn";
  out.push_back(CommanderWords::kMeter + intStr(d.meterMax) + ": " + rule);
  return out;
}

}  // namespace ote
