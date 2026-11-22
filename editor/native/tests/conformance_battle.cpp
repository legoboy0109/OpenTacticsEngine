// Conformance tables, part 6: the battle scene's art (data/graphics.json "battle", editor/schema/_battle.json). The fixture is a copy
// of demo-srpg (it has the sheets, unit types, terrain, weapon groups and sound ids the keys point at) with a small valid "battle"
// section written into its graphics.json; each context changes that base to suit the keys it claims (a travel effect for the keys only a
// travel effect takes, a screen layer for `order`, an assignment-only entry for `effects` and `byType`).
#include "conformance_contexts.h"

namespace conformance {

namespace {

// Every animation slot of a set; the first group takes holdFrame, the second (the dodges) also startFrame and startDelayMs.
const char* const kSlots[] = {"stand", "attack", "crit", "ranged", "critRanged", "cast", "miss", "dodge", "dodgeRanged", "hurt", "defeat", "standRanged"};
const char* const kHoldSlots[] = {"attack", "crit", "ranged", "critRanged", "cast", "miss", "hurt"};
const char* const kDodgeSlots[] = {"dodge", "dodgeRanged"};

const char* const kAnim = R"({"sheet":"units","frames":[[0,0]]})";
const char* const kAnimStart = R"({"sheet":"units","frames":[[0,0]],"startFrame":0})";

// A small valid battle section. The stage is tiny on purpose: the stage keys are tried at the ends of their ranges (16 pixels),
// and every fighter point has to stay inside it.
QString baseBattle(const QString& effects) {
  return QStringLiteral(R"({"sets":{"hero":{"animations":{"stand":%1,"attack":%1,"dodge":%2}}},"effects":%3,)"
                        R"("assign":[{"set":"hero","type":"lord"}],)"
                        R"("backgrounds":{"meadow":{"sheet":"tiles","cell":[0,0]}},)"
                        R"("platforms":{"grass":{"sheet":"tiles","cell":[0,0],"far":[1,0]}},)"
                        R"("terrainBackground":{"plain":"meadow"},"terrainPlatform":{"plain":"grass"},"defaultBackground":"meadow","defaultPlatform":"grass",)"
                        R"("stage":{"width":64,"height":48,"feet":{"left":[4,12],"right":[14,12]},"platform":{"left":[4,14],"right":[14,14]},)"
                        R"("ranges":[{"from":4,"feet":{"left":[3,12],"right":[13,12]}}]},"scene":{"resultHoldMs":100}})")
      .arg(kAnim, kAnimStart, effects);
}

const char* const kPlainFx = R"([{"id":"fx1","sheet":"units","frames":[[0,0]]}])";
const char* const kTravelFx = R"([{"id":"fx1","sheet":"units","frames":[[0,0]],"mode":"travel"}])";
const char* const kScreenFx = R"([{"id":"fx1","sheet":"units","frames":[[0,0]],"layer":"screen"}])";
const char* const kListFx = R"([{"weapon":"iron_sword","effects":"fx1","byType":{"lord":"fx1"}},{"id":"fx1","sheet":"units","frames":[[0,0]]}])";

Context make(const char* name, const char* effects, std::vector<const char*> claims, std::map<QString, QString> overrides) {
  return {name, "demo-srpg", {{"data/graphics.json", "battle", baseBattle(QString::fromUtf8(effects))}}, std::move(claims), std::move(overrides)};
}

}  // namespace

void addBattleContexts(std::vector<Context>& c) {
  const QString p = "graphics:battle.sets.*.animations.";
  std::map<QString, QString> o = {
      {"graphics:battle.assign", R"([{"set":"hero","type":"lord"}])"},
      {"graphics:battle.effects", kPlainFx},
      {"graphics:battle.stage.ranges[].pan.leftView", "1"},
      {"graphics:battle.stage.ranges[].pan.rightView", "1"},
      // The effect's own timing keys: frame numbers stay inside the one frame the base effect has.
      {"graphics:battle.effects[].hit", "0"},
      {"graphics:battle.effects[].spawn", "0"},
  };
  for (const char* slot : kSlots) {
    const QString block = p + slot;
    const bool dodge = QString(slot) == "dodge" || QString(slot) == "dodgeRanged";
    o[block] = dodge ? kAnimStart : kAnim;
    o[block + ".hit"] = "0";
    o[block + ".spawn"] = "0";
  }
  for (const char* slot : kHoldSlots) o[p + slot + ".holdFrame"] = "0";
  for (const char* slot : kDodgeSlots) {
    o[p + slot + ".holdFrame"] = "0";
    o[p + slot + ".startFrame"] = "0";
  }
  c.push_back(make("battle-main", kPlainFx, {"graphics:battle"}, o));

  // An entry with a weapon that only names library effects (`effects`, `byType`), and the library effect it names.
  c.push_back(make("battle-effect-lists", kListFx, {"graphics:battle.effects[].effects", "graphics:battle.effects[].byType"},
                   {{"graphics:battle.effects", kListFx}}));
  // Keys only a travelling effect takes (`speed` also needs the fixed stage the base has).
  c.push_back(make("battle-travel", kTravelFx,
                   {"graphics:battle.effects[].returns", "graphics:battle.effects[].lingerMs", "graphics:battle.effects[].hitAfterMs", "graphics:battle.effects[].endOffset",
                    "graphics:battle.effects[].speed", "graphics:battle.effects[].atRange[].endOffset"},
                   {{"graphics:battle.effects", kTravelFx}}));
  // Keys only a screen layer takes.
  c.push_back(make("battle-screen", kScreenFx, {"graphics:battle.effects[].order", "graphics:battle.effects[].mirror"},
                   {{"graphics:battle.effects", kScreenFx}}));
}

void addBattleExtraSkips(std::map<QString, QString>& s) { (void)s; }

}  // namespace conformance
