#include "battle/condition_text.h"

#include "ui/text.h"

namespace ote {

std::string conditionName(const Database& db, int condition) {
  return condition >= 0 && (size_t)condition < db.conditions.size() ? db.conditions[(size_t)condition].name : "condition";
}

std::string conditionTag(const Database& db, int condition) {
  return condition >= 0 && (size_t)condition < db.conditions.size() ? db.conditions[(size_t)condition].tag : "?";
}

std::vector<std::string> conditionTags(const Battle& b, const Unit& u) {
  std::vector<std::string> out;
  for (const ConditionState& cs : u.conditions) out.push_back(conditionTag(*b.db, cs.condition));
  return out;
}

std::string conditionDurationText(const Database& db, const ConditionState& cs) {
  if (cs.condition < 0 || (size_t)cs.condition >= db.conditions.size()) return "";
  const ConditionDef& d = db.conditions[(size_t)cs.condition];
  if (d.until == ConditionUntil::Cured) return ConditionWords::kUntilCured;
  if (d.until == ConditionUntil::MapEnd) return ConditionWords::kUntilMapEnd;
  if (cs.left <= 0) return ConditionWords::kLastTurn;
  return intStr(cs.left) + " " + (cs.left == 1 ? ConditionWords::kTurn : ConditionWords::kTurns) + " left";
}

std::string conditionDurationShort(const Database& db, const ConditionState& cs) {
  if (cs.condition < 0 || (size_t)cs.condition >= db.conditions.size()) return "";
  const ConditionDef& d = db.conditions[(size_t)cs.condition];
  if (d.until == ConditionUntil::Cured) return ConditionWords::kUntilCuredShort;
  if (d.until == ConditionUntil::MapEnd) return ConditionWords::kUntilMapEndShort;
  return cs.left <= 0 ? ConditionWords::kLastTurnShort : intStr(cs.left);
}

std::string conditionAppliedText(const Database& db, int condition) { return conditionName(db, condition) + "!"; }
std::string conditionCuredText(const Database& db, int condition) { return std::string("Cured ") + conditionName(db, condition); }

std::string conditionMissedText(const Database& db, int condition) { return conditionName(db, condition) + " missed"; }

std::string conditionChanceText(const Database& db, int condition, int chance) {
  if (chance < 0 || condition < 0) return "";
  return conditionName(db, condition) + " " + std::to_string(chance) + "%";
}

std::string conditionEndedText(const Database& db, int condition, int why) {
  if (why == kConditionCured) return conditionCuredText(db, condition);
  if (why == kConditionReplaced) return "";  // the new condition says it all
  return conditionName(db, condition) + " ended";  // ran out, wore off early, or the strike woke the unit
}

std::string conditionTickText(const Database& db, int condition, int what) {
  if (what == kTickActsAlone) return conditionName(db, condition) + "!";
  return conditionName(db, condition);  // the turn is skipped
}

std::string unitStoppedText(const Battle& b, const Unit& u) {
  int c = b.conditionBlock(u, ConditionBlock::Act);
  if (c >= 0) return conditionName(*b.db, c) + ": " + ConditionWords::kCannotAct;
  c = b.conditionBlock(u, ConditionBlock::Berserk);
  if (c >= 0) return conditionName(*b.db, c) + ": " + ConditionWords::kActsAlone;
  return "";
}

std::string unitStuckText(const Battle& b, const Unit& u) {
  const int c = b.conditionBlock(u, ConditionBlock::Move);
  return c >= 0 ? conditionName(*b.db, c) + ": " + ConditionWords::kCannotMove : "";
}

std::vector<std::string> staffConditionLines(const Battle& b, const StaffHit& h) {
  std::vector<std::string> out;
  const Database& db = *b.db;
  const std::string name = b.units[(size_t)h.unit].name;
  if (h.inflict >= 0) {
    std::string line = name + "  " + conditionName(db, h.inflict);
    if (h.inflictTurns > 0) line += " " + intStr(h.inflictTurns) + " " + (h.inflictTurns == 1 ? ConditionWords::kTurn : ConditionWords::kTurns);
    out.push_back(line);
  }
  if (!h.cures.empty()) {
    std::string line = std::string(ConditionWords::kCures) + " ";
    for (size_t i = 0; i < h.cures.size(); ++i) line += (i ? ", " : "") + conditionName(db, h.cures[i]);
    out.push_back(name + "  " + line);
  }
  return out;
}

}  // namespace ote
