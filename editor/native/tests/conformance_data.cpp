// Conformance tables, part 2: the keys of the core data files beyond the basics (weapons, units, items, terrain) and the
// skill language (skills, conditions, abilities). Skill blocks are probed with an extra skill whose block has the `when`
// the effect or condition belongs to: the loader refuses an effect in the wrong kind of block.
#include "conformance_contexts.h"

namespace conformance {

namespace {

// A probe skill appended to skills.json: its last row is where the table tests place their samples.
Op probeSkill(const char* when, const char* firstEffect) {
  return {"data/skills.json", "skills[+]",
          QString(R"({"id":"probe","name":"Probe","tags":["flavour"],"blocks":[{"when":"%1","do":[%2]}]})").arg(when, firstEffect)};
}

// The same, for a probe status condition (data/conditions.json takes the blocks of a skill).
void conditionBlockContext(std::vector<Context>& c, const char* name, const char* when, const char* effect,
                           std::vector<const char*> claims, std::map<QString, QString> overrides = {}) {
  c.push_back({name, "srpg-conditions",
               {{"data/conditions.json", "conditions[+]",
                 QString(R"({"id":"probe","name":"Probe","turns":3,"blocks":[{"when":"%1","do":[%2]}]})").arg(when, effect)}},
               std::move(claims), std::move(overrides)});
}

void skillContext(std::vector<Context>& c, const char* name, const char* fixture, const char* when, const char* effect,
                  std::vector<const char*> claims, std::map<QString, QString> overrides = {}) {
  c.push_back({name, fixture, {probeSkill(when, effect)}, std::move(claims), std::move(overrides)});
}

}  // namespace

void addDataContexts(std::vector<Context>& c) {
  // --- weapons ---
  c.push_back({"weapon-ammo", "area-fire", {}, {"weapons:ammo"}, {}});
  c.push_back({"weapon-skills", "srpg-skills", {}, {"weapons:skills"}, {}});
  c.push_back({"weapon-inflict", "srpg-conditions", {}, {"weapons:inflict"}, {{"weapons:inflict", R"({"condition":"poison"})"}}});
  c.push_back({"weapon-rank", "srpg-ranks", {{"data/weapons.json", "weapons[-1].group", "\"sword\""}}, {"weapons:rank", "weapons:rankXp"}, {}});
  c.push_back({"weapon-versus", "plain-economy", {}, {"weapons:versus"}, {{"weapons:versus", R"({"infantry":5})"}}});

  // --- unit types ---
  c.push_back({"unit-abilities", "srpg-abilities", {}, {"units:abilities"}, {}});
  c.push_back({"unit-skills", "srpg-skills", {}, {"units:skills", "units:learns"}, {}});
  c.push_back({"unit-promotions", "srpg-locks", {}, {"units:promotions"}, {}});
  c.push_back({"unit-ranks", "srpg-ranks", {}, {"units:ranks", "units:caps"}, {{"units:ranks", R"({"sword":"e"})"}}});
  c.push_back({"unit-pair", "srpg-pair", {}, {"units:pairBonus"}, {{"units:pairBonus", R"({"str":1})"}}});
  c.push_back({"unit-vision", "mapchange", {}, {"units:vision"}, {}});
  c.push_back({"unit-fuel", "plain-economy", {{"data/units.json", "units[-1].fuel", "50"}}, {"units:fuel", "units:fuelPerTurn", "units:supplies"}, {}});
  c.push_back({"unit-items", "srpg-chars", {}, {"units:items"}, {}});

  // --- items by kind ---
const Op probeUsable = {"data/items.json", "items[+]", R"({"id":"probe","name":"Probe","kind":"usable","heal":5})"};
  c.push_back({"item-usable", "srpg-conditions", {probeUsable}, {"items:heal", "items:raise", "items:promotes", "items:cure", "items:inflict"}, {}});
  c.push_back({"item-opens", "srpg-doors", {probeUsable}, {"items:opens"}, {}});
  c.push_back({"item-light", "mapchange", {probeUsable}, {"items:light"}, {}});
  c.push_back({"item-weapon", "srpg-skills", {{"game.json", "rules.items.weapons", "\"carried\""}, {"data/items.json", "items[+]", R"({"id":"probe","name":"Probe","kind":"weapon","weapon":"sword"})"}}, {"items:weapon"}, {}});
  c.push_back({"item-accessory", "srpg-skills", {{"data/items.json", "items[+]", R"({"id":"probe","name":"Probe","kind":"accessory","bonus":{"def":1}})"}}, {"items:bonus", "items:skills"}, {}});
  const Op probeStaff = {"data/items.json", "items[+]", R"({"id":"probe","name":"Probe","kind":"staff","group":"heal","hit":80,"effects":[{"heal":5}]})"};
  c.push_back({"item-staff", "srpg-conditions", {probeStaff},
               {"items:target", "items:group", "items:range", "items:area", "items:effects", "items:hit", "items:xp", "items:xpMiss", "items:if",
                "items:wieldedByTypes"},
               {}});
  // A staff that lights a square or opens a lock picks no unit: no hit roll, no target.
  c.push_back({"item-staff-square", "srpg-conditions", {{"data/items.json", "items[+]", R"({"id":"probe","name":"Probe","kind":"staff","group":"heal","effects":[{"heal":5}]})"}},
               {"items:effects[]{light}", "items:effects[]{unlock}"}, {}});
  c.push_back({"item-staff-wield", "srpg-staves", {probeStaff}, {"items:wieldedBy"}, {}});
  c.push_back({"item-staff-rank", "srpg-ranks", {{"data/items.json", "items[+]", R"({"id":"probe","name":"Probe","kind":"staff","group":"staff","effects":[{"heal":5}]})"}},
               {"items:rank", "items:rankXp"}, {}});

  // --- terrain ---
  c.push_back({"terrain-fog", "demo-economy", {}, {"terrain:visionBonus", "terrain:hides", "terrain:blocksSight"}, {}});

  // --- skills: the blocks and the language inside them ---
  skillContext(c, "skill-block", "srpg-conditions", "strike", R"({"hit":5})",
               {"skills:blocks", "skills:id", "skills:name", "skills:description", "skills:tags"},
               {{"skills:blocks[].when", "\"strike\""}, {"skills:blocks[].do", R"([{"hit":5}])"}, {"skills:blocks", R"([{"when":"strike","do":[{"hit":5}]}])"}});
  skillContext(c, "skill-cond", "srpg-conditions", "strike", R"({"hit":5})", {"skills:blocks[].if"});
  skillContext(c, "skill-cond-arc", "facing", "strike", R"({"hit":5})", {"skills:blocks[].if[]{arc}"});
  skillContext(c, "skill-cond-damage-type", "damage-types", "strike", R"({"hit":5})", {"skills:blocks[].if[]{damageType}", "skills:blocks[].if[]{weaponKind}"});
  skillContext(c, "skill-cond-structure", "srpg-skills", "strike", R"({"hit":5})", {"skills:blocks[].if[]{structure}"});
  skillContext(c, "skill-aura-kind", "damage-types", "combat", R"({"crit":5})", {"skills:blocks[].aura.weaponKind"});
  skillContext(c, "skill-chance", "srpg-conditions", "strike", R"({"hit":5})", {"skills:blocks[].chance"});
  skillContext(c, "skill-cond-condition", "srpg-conditions", "strike", R"({"hit":5})", {"skills:blocks[].if[]{condition}"});
  skillContext(c, "skill-aura", "srpg-conditions", "combat", R"({"crit":5})", {"skills:blocks[].aura"});

  skillContext(c, "skill-fx-always", "srpg-conditions", "always", R"({"stats":{"mov":1}})",
               {"skills:blocks[].do[]{stats}", "skills:blocks[].do[]{growths}", "skills:blocks[].do[]{maxMoveCost}", "skills:blocks[].do[]{pass}",
                "skills:blocks[].do[]{moveAgain}", "skills:blocks[].do[]{xpPercent}", "skills:blocks[].do[]{weaponXpPercent}",
                "skills:blocks[].do[]{noCarryPenalty}", "skills:blocks[].do[]{cancelEffective}", "skills:blocks[].do[]{staffRange}",
                "skills:blocks[].do[]{carryBonus}", "skills:blocks[].do[]{vision}"},
               {{"skills:blocks[].do[]{stats}.stats", R"({"str":1})"}, {"skills:blocks[].do[]{growths}.growths", R"({"str":1})"}});
  skillContext(c, "skill-fx-ability", "srpg-abilities", "always", R"({"ability":"yank"})", {"skills:blocks[].do[]{ability}"});
  skillContext(c, "skill-fx-immune", "srpg-conditions", "always", R"({"immune":["poison"]})", {"skills:blocks[].do[]{immune}"});
  skillContext(c, "skill-fx-combat", "srpg-conditions", "combat", R"({"crit":5})",
               {"skills:blocks[].do[]{avoid}", "skills:blocks[].do[]{critAvoid}", "skills:blocks[].do[]{damageTaken}",
                "skills:blocks[].do[]{damageTakenPercent}", "skills:blocks[].do[]{strikes}", "skills:blocks[].do[]{multiStrike}",
                "skills:blocks[].do[]{strikeFirst}", "skills:blocks[].do[]{counterAnyRange}", "skills:blocks[].do[]{noCounter}",
                "skills:blocks[].do[]{followUp}", "skills:blocks[].do[]{followShift}", "skills:blocks[].do[]{trianglePercent}",
                "skills:blocks[].do[]{nullifySkills}", "skills:blocks[].do[]{stats}.stats.*{formula}"},
               {{"skills:blocks[].do[]{stats}.stats", R"({"str":1})"},
                {"skills:blocks[].do[]{nullifySkills}.nullifySkills{skills with a tag}.tag", "\"flavour\""}});
  skillContext(c, "skill-fx-strike", "srpg-conditions", "strike", R"({"hit":5})",
               {"skills:blocks[].do[]{hit}", "skills:blocks[].do[]{crit}", "skills:blocks[].do[]{damage}", "skills:blocks[].do[]{damagePercent}",
                "skills:blocks[].do[]{drain}", "skills:blocks[].do[]{strikeAgain}", "skills:blocks[].do[]{recoil}", "skills:blocks[].do[]{mark}"},
               {});
  skillContext(c, "skill-fx-inflict", "srpg-conditions", "strike", R"({"hit":5})", {"skills:blocks[].do[]{inflict}"},
               {{"skills:blocks[].do[]{inflict}.inflict", R"({"condition":"poison"})"}});
  skillContext(c, "skill-fx-struck", "srpg-conditions", "struck", R"({"avoid":5})", {"skills:blocks[].do[]{survive}"});
  skillContext(c, "skill-fx-turn", "srpg-conditions", "turnStart", R"({"heal":1})",
               {"skills:blocks[].do[]{heal}", "skills:blocks[].do[]{hurt}", "skills:blocks[].do[]{cure}"});
  skillContext(c, "skill-fx-after", "srpg-conditions", "afterCombat", R"({"heal":1})", {"skills:blocks[].do[]{actAgain}"});

  // --- conditions (status effects): the same blocks as skills, in the same shapes ---
  const Op probeCondition = {"data/conditions.json", "conditions[+]", R"({"id":"probe","name":"Probe","turns":3,"blocks":[{"when":"always","do":[{"stats":{"str":1}}]}]})"};
  c.push_back({"condition-rows", "srpg-conditions", {probeCondition},
               {"conditions:id", "conditions:name", "conditions:description", "conditions:tag", "conditions:turns", "conditions:until",
                "conditions:wearOff", "conditions:endsWhenStruck", "conditions:replaces", "conditions:scale", "conditions:blocks", "conditions:kind"},
               {{"conditions:blocks", R"([{"when":"always","do":[{"stats":{"str":1}}]}])"}, {"conditions:blocks[].do", R"([{"stats":{"str":1}}])"}}});
  conditionBlockContext(c, "cond-cond", "strike", R"({"hit":5})", {"conditions:blocks[].if"});
  conditionBlockContext(c, "cond-chance", "strike", R"({"hit":5})", {"conditions:blocks[].chance"});
  conditionBlockContext(c, "cond-aura", "combat", R"({"crit":5})", {"conditions:blocks[].aura"});
  conditionBlockContext(c, "cond-fx-always", "always", R"({"stats":{"mov":1}})",
                        {"conditions:blocks[].do[]{stats}", "conditions:blocks[].do[]{growths}", "conditions:blocks[].do[]{maxMoveCost}",
                         "conditions:blocks[].do[]{pass}", "conditions:blocks[].do[]{moveAgain}", "conditions:blocks[].do[]{xpPercent}",
                         "conditions:blocks[].do[]{weaponXpPercent}", "conditions:blocks[].do[]{noCarryPenalty}",
                         "conditions:blocks[].do[]{cancelEffective}", "conditions:blocks[].do[]{vision}", "conditions:blocks[].do[]{immune}",
                         "conditions:blocks[].do[]{cannotAct}", "conditions:blocks[].do[]{cannotMove}",
                         "conditions:blocks[].do[]{forbid}", "conditions:blocks[].do[]{berserk}"},
                        {{"conditions:blocks[].do[]{stats}.stats", R"({"str":1})"}, {"conditions:blocks[].do[]{growths}.growths", R"({"str":1})"}});
  conditionBlockContext(c, "cond-fx-combat", "combat", R"({"crit":5})",
                        {"conditions:blocks[].do[]{avoid}", "conditions:blocks[].do[]{critAvoid}", "conditions:blocks[].do[]{damageTaken}",
                         "conditions:blocks[].do[]{damageTakenPercent}", "conditions:blocks[].do[]{strikes}", "conditions:blocks[].do[]{multiStrike}",
                         "conditions:blocks[].do[]{strikeFirst}", "conditions:blocks[].do[]{counterAnyRange}", "conditions:blocks[].do[]{noCounter}",
                         "conditions:blocks[].do[]{followUp}", "conditions:blocks[].do[]{followShift}", "conditions:blocks[].do[]{trianglePercent}",
                         "conditions:blocks[].do[]{stats}.stats.*{formula}"},
                        {{"conditions:blocks[].do[]{stats}.stats", R"({"str":1})"}});
  conditionBlockContext(c, "cond-fx-strike", "strike", R"({"hit":5})",
                        {"conditions:blocks[].do[]{hit}", "conditions:blocks[].do[]{crit}", "conditions:blocks[].do[]{damage}",
                         "conditions:blocks[].do[]{damagePercent}", "conditions:blocks[].do[]{drain}", "conditions:blocks[].do[]{strikeAgain}",
                         "conditions:blocks[].do[]{recoil}", "conditions:blocks[].do[]{mark}", "conditions:blocks[].do[]{inflict}"},
                        {{"conditions:blocks[].do[]{inflict}.inflict", R"({"condition":"poison"})"}});
  conditionBlockContext(c, "cond-fx-struck", "struck", R"({"avoid":5})", {"conditions:blocks[].do[]{survive}"});
  conditionBlockContext(c, "cond-fx-turn", "turnStart", R"({"heal":1})",
                        {"conditions:blocks[].do[]{heal}", "conditions:blocks[].do[]{hurt}", "conditions:blocks[].do[]{cure}"});
  conditionBlockContext(c, "cond-fx-after", "afterCombat", R"({"heal":1})", {"conditions:blocks[].do[]{actAgain}"});

  // --- abilities: what an ability may do depends on whom it targets ---
  const auto ability = [](const char* target, const char* range, const char* effect) {
    return Op{"data/abilities.json", "abilities[+]",
              QString(R"({"id":"probe","name":"Probe","target":"%1","range":%2,"effect":%3})").arg(target, range, effect)};
  };
  c.push_back({"ability-rows", "srpg-abilities", {ability("unit", "[1,2]", R"({"heal":5})")},
               {"abilities:id", "abilities:name", "abilities:description", "abilities:target", "abilities:range", "abilities:cost",
                "abilities:uses", "abilities:cooldown", "abilities:afterMove", "abilities:xp", "abilities:if"},
               {{"abilities:target", "\"unit\""}}});
  c.push_back({"ability-inflict", "speed-order", {ability("unit", "[1,2]", R"({"heal":5})")}, {"abilities:inflict"},
               {{"abilities:inflict{one condition}.condition", "\"poison\""}, {"abilities:inflict{several conditions}[].condition", "\"poison\""}}});
  // Areas and several effects (docs/SRPG.md "Areas and several effects for abilities"): an area needs a heal / cure / inflict-only ability aimed at
  // ally, enemy or unit; "length" belongs to a line and "vertical" to a game with height rules, so each has its own context.
  const auto areaAbility = [](const char* area) {
    return Op{"data/abilities.json", "abilities[+]",
              QString(R"({"id":"probe","name":"Probe","target":"unit","range":[0,2],"effect":{"heal":5},"area":%1})").arg(area)};
  };
  c.push_back({"ability-area-line", "srpg-abilities", {areaAbility(R"({"shape":"line","length":2})")}, {"abilities:area.length"}, {}});
  c.push_back({"ability-area-vertical", "speed-order",
               {{"game.json", "rules.height", R"({"jump":{"default":1}})"}, areaAbility(R"({"shape":"radius","radius":1})")},
               {"abilities:area.vertical"}, {}});
  c.push_back({"ability-area", "srpg-abilities", {areaAbility(R"({"shape":"radius","radius":1})")}, {"abilities:area"},
               {{"abilities:area", R"({"shape":"radius","radius":1})"}, {"abilities:area.shape", "\"radius\""}}});
  c.push_back({"ability-heal-cure", "speed-order", {ability("unit", "[1,2]", R"({"heal":5,"cure":["poison"]})")}, {"abilities:effect{heal and cure}"},
               {{"abilities:effect{heal and cure}.cure", R"(["poison"])"}}});
  c.push_back({"ability-adjacent", "srpg-abilities", {ability("unit", "[1,1]", R"({"push":1})")},
               {"abilities:effect{push}", "abilities:effect{pull}", "abilities:effect{swap}", "abilities:effect{moveBehind}",
                "abilities:effect{movePast}", "abilities:effect{heal}"},
               {}});
  c.push_back({"ability-steal", "srpg-abilities", {ability("enemy", "[1,1]", R"({"push":1})")}, {"abilities:effect{steal}"}, {}});
  c.push_back({"ability-ally", "speed-order", {ability("ally", "[1,1]", R"({"push":1})")}, {"abilities:effect{refresh}", "abilities:effect{cure}"},
               {{"abilities:effect{cure}.cure", R"(["poison"])"}}});
  c.push_back({"ability-cell", "srpg-abilities", {ability("cell", "[1,2]", R"({"summon":{"type":"hound"}})")}, {"abilities:effect{summon}"}, {}});
  c.push_back({"ability-summon-items", "demo-srpg", {ability("cell", "[1,2]", R"({"summon":{"type":"hound"}})")},
               {"abilities:effect{summon}.summon.items", "abilities:effect{summon}.summon.pick"},
               {{"abilities:effect{summon}.summon", R"({"type":"hound","items":[{"item":"salve"}]})"}, {"abilities:effect{summon}.summon.items", R"([{"item":"salve"}])"}}});
  c.push_back({"ability-unlock", "srpg-doors", {ability("cell", "[1,2]", R"({"unlock":true})")}, {"abilities:effect{unlock}"}, {}});
  c.push_back({"ability-charge", "speed-order", {{"data/abilities.json", "abilities[+]", R"({"id":"probe","name":"Probe","target":"unit","range":[1,2],"effect":{"heal":5},"cost":"action","chargeSpeed":20})"}},
               {"abilities:chargeSpeed", "abilities:chargeTargets", "abilities:interruptedBy"}, {}});
  // A strike takes its targets and reach from the weapon, so the probe has no "target" / "range".
  c.push_back({"ability-strike", "srpg-abilities", {{"data/abilities.json", "abilities[+]", R"({"id":"probe","name":"Probe","effect":{"strike":"bow"},"cost":"action"})"}},
               {"abilities:effect{strike}"}, {{"abilities:effect{strike}", R"({"strike":"bow"})"}, {"abilities:effect{strike}.strike", "\"bow\""}}});
  // Unit resources (MP and the like) and what an ability spends of them.
  const char* kPools = R"([{"id":"mp","name":"MP","max":"atk"}])";
  c.push_back({"unit-resources", "srpg-abilities", {{"game.json", "unitResources", kPools}, ability("unit", "[1,2]", R"({"heal":5})")},
               {"game:unitResources", "abilities:spend"}, {{"game:unitResources", kPools}, {"abilities:spend", R"({"mp":2})"}}});
  // Learning from equipment: ability points are refused unless an item teaches, so an accessory teaching a skill is added (the last row, where the
  // keys are placed). An entry teaches a skill or an ability, never both, so the ability is tried on its own in a game with menu abilities.
  const char* kTeachSkill = R"([{"skill":"iron_will","points":20}])";
  c.push_back({"item-teaches", "srpg-conditions",
               {{"data/items.json", "items[+]", QString(R"({"id":"charm","name":"Charm","kind":"accessory","teaches":%1})").arg(kTeachSkill)},
                {"game.json", "rules.abilityPoints", R"({"perBattle":10})"}},
               {"items:teaches", "game:rules.abilityPoints"},
               {{"items:teaches", kTeachSkill}, {"items:teaches[].skill", "\"iron_will\""}, {"game:rules.abilityPoints", R"({"perBattle":10})"}}});
  c.push_back({"item-teaches-ability", "srpg-doors",
               {{"data/items.json", "items[+]", R"({"id":"charm","name":"Charm","kind":"accessory","teaches":[{"ability":"pick","points":15}]})"}},
               {"items:teaches[].ability"}, {{"items:teaches[].ability", "\"pick\""}}});

  // --- structures ---
  c.push_back({"structure-rows", "plain-economy", {},
               {"structures:id", "structures:name", "structures:color", "structures:glyph", "structures:width", "structures:height", "structures:moveCost",
                "structures:props", "structures:tags", "structures:capturePoints", "structures:income", "structures:produces", "structures:research",
                "structures:repairs", "structures:blocksSight", "structures:light"},
               {}});
  c.push_back({"structure-hex", "hex-flat", {}, {"structures:rings"}, {}});
  c.push_back({"structure-upgrades", "research-branches", {}, {"structures:upgrades"}, {}});
  c.push_back({"structure-sells", "demo-srpg", {}, {"structures:sells"}, {}});
  c.push_back({"structure-door", "srpg-doors", {{"data/structures.json", "structures[+]", R"({"id":"probe","name":"Probe","moveCost":{"foot":0},"door":{"opens":"cell_door_open","hp":10,"by":["key"]}})"}},
               {"structures:door"}, {{"structures:door", R"({"opens":"cell_door_open","hp":10,"by":["key"]})"}, {"structures:door.opens", "\"cell_door_open\""}, {"structures:door.keys", R"(["brass_key"])"}}});
  c.push_back({"structure-chest", "srpg-doors", {{"data/structures.json", "structures[+]", R"({"id":"probe","name":"Probe","chest":{"opens":"chest_open"}})"}},
               {"structures:chest"}, {{"structures:chest", R"({"opens":"chest_open"})"}, {"structures:chest.opens", "\"chest_open\""}, {"structures:chest.keys", R"(["master_key"])"}}});
  c.push_back({"structure-visit", "srpg-doors", {{"data/structures.json", "structures[+]", R"({"id":"probe","name":"Probe","visit":{}})"}},
               {"structures:visit"}, {{"structures:visit", "{}"}, {"structures:visit.becomes", "\"hamlet_shut\""}, {"structures:visit.ruin", "\"hamlet_ruin\""}}});
  c.push_back({"structure-weapon", "srpg-mapweapon", {{"data/structures.json", "structures[+]", R"({"id":"probe","name":"Probe","weapon":"ballista_bolt"})"}},
               {"structures:weapon", "structures:uses", "structures:ignoreRank", "structures:by", "structures:becomes"},
               {{"structures:becomes", "\"ballista_wreck\""}}});

  // --- maps: the parts outside the tile grid ---
  c.push_back({"map-rows", "ai-scripted", {}, {"maps:name", "maps:grid", "maps:units[].name", "maps:units[].level", "maps:units[].items", "maps:units[].stats"},
               {{"maps:grid.width", "14"}, {"maps:grid.height", "6"}}});
  c.push_back({"map-fog", "ai-scripted", {{"maps/field.json", "fog", "true"}}, {"maps:fog", "maps:fogVision"}, {}});
  c.push_back({"map-ai-sees", "ai-scripted", {{"maps/field.json", "fog", "true"}}, {"maps:aiSeesAll"}, {}});
  c.push_back({"map-structures", "plain-economy", {}, {"maps:structures"}, {{"maps:structures[].side", "\"blue\""}}});
  c.push_back({"map-structures-doors", "srpg-doors", {}, {"maps:structures[].locked", "maps:structures[].contents", "maps:structures[].visitSides", "maps:structures[].visitUnits"}, {}});
  c.push_back({"map-structures-weapon", "srpg-mapweapon", {}, {"maps:structures[].uses"}, {}});

  // --- characters, supports, ranks, caps ---
  c.push_back({"character-rows", "srpg-chars", {},
               {"characters:id", "characters:name", "characters:description", "characters:type", "characters:level", "characters:stats", "characters:growths",
                "characters:tags", "characters:wields", "characters:promotion", "characters:items"},
               {}});
  c.push_back({"character-skills", "srpg-skills", {}, {"characters:skills", "characters:learns"}, {}});
  c.push_back({"character-abilities", "srpg-abilities", {}, {"characters:abilities"}, {}});
  c.push_back({"character-ranks", "srpg-ranks", {}, {"characters:ranks", "characters:capAdjust"}, {{"characters:ranks", R"({"sword":"e"})"}}});
  c.push_back({"character-resist", "damage-types", {}, {"characters:resist"}, {}});
  c.push_back({"character-jobs", "jobs", {}, {"characters:equipped", "characters:job", "characters:jobs"}, {}});
  c.push_back({"character-quotes", "srpg-quotes", {}, {"characters:quotes"}, {}});
  c.push_back({"support-rows", "srpg-supports", {}, {"supports:"},
               {{"supports:rules", R"({"levelNames":["C","B"]})"}, {"supports:rules.levelNames", R"(["C","B"])"},
                {"supports:pairs", R"([{"a":"eirik","b":"bram","levels":[{"points":10},{"points":20}]}])"},
                {"supports:pairs[].b", "\"bram\""},
                {"supports:pairs[].levels[].bonus.stats", R"({"str":1})"},
                {"supports:pairs[].levels[].talk", R"([{"say":"Hello","speaker":"eirik"}])"}}});
  c.push_back({"rank-rows", "srpg-ranks", {}, {"ranks:", "caps:"},
               {{"ranks:ranks", R"([{"id":"e","xp":0},{"id":"d","xp":3},{"id":"c","xp":7},{"id":"b","xp":12}])"}}});
}

void addDataExtraSkips(std::map<QString, QString>& s) {
  s.insert({
      {"maps:grid.type = \"hex\"", "a square game's maps are square (cross-file rule)"},
      {"maps:grid.width = 1", "the tile rows must have exactly that many characters (cross-key rule; the tile grid is raw JSON for now)"},
      {"maps:grid.width = 256", "the tile rows must have exactly that many characters (cross-key rule; the tile grid is raw JSON for now)"},
      {"maps:grid.height = 1", "the tile grid must have exactly that many rows (cross-key rule; the tile grid is raw JSON for now)"},
      {"maps:grid.height = 256", "the tile grid must have exactly that many rows (cross-key rule; the tile grid is raw JSON for now)"},
  });
  s.insert({
      {"characters:level = 99", "the level cap of the game (rules.progression.maxLevel) is lower"},
      {"characters:learns[].level = 99", "the level cap of the game (rules.progression.maxLevel) is lower"},
      {"characters:promotion.fromLevel = 99", "the level cap of the game (rules.progression.maxLevel) is lower"},
      {"ranks:ranks[].xp = 9999", "the first rank must start at 0 (cross-key rule)"},
      {"structures:width = 8", "the maps of the fixture have structures that no longer fit (cross-file rule)"},
      {"structures:height = 8", "the maps of the fixture have structures that no longer fit (cross-file rule)"},
      {"supports:pairs[].levels[].points = 1000000", "the second level must have more points than the first (cross-key rule)"},
  });
  for (const char* schema : {"skills", "conditions"})  // which effects a block may hold depends on its `when` (cross-key rule)
    for (const char* when : {"always", "combat", "strike", "struck", "turnStart", "defeat", "afterCombat", "staffUse"})
      s.insert({QString("%1:blocks[].when = \"%2\"").arg(schema, when), "the effects of the block belong to one kind of block (cross-key rule); every kind is tried with its own effects"});
  // "to": "foe" belongs to combat blocks, "enemies" / "foe" of hurt to turnStart / afterCombat (cross-key rule)
  for (const char* schema : {"skills", "conditions"}) {
    for (const char* fx : {"stats", "hit", "crit", "damage", "avoid", "critAvoid", "damageTaken", "strikes", "followUp", "followShift", "trianglePercent"})
      s.insert({QString("%1:blocks[].do[]{%2}.to = \"foe\"").arg(schema, fx), "\"to\": \"foe\" belongs to combat blocks only (cross-key rule)"});
    s.insert({QString("%1:blocks[].do[]{hurt}.to = \"foe\"").arg(schema), "\"foe\" belongs to afterCombat blocks, \"enemies\" and \"self\" to turnStart (cross-key rule)"});
    s.insert({QString("%1:blocks[].do[]{heal}.to = \"allies\"").arg(schema), "\"allies\" is not allowed in staffUse blocks (cross-key rule)"});
  }
  s.insert({
      {"items:opens = [\"zz_no_such_id\"]", "the problem is reported in data/structures.json, where the kinds are resolved"},
      {"items:target = \"foe\"", "a staff that heals cannot target the foe (cross-key rule)"},
      {"items:effects[]{light}.light.shrink = 9", "\"shrink\" must be less than the radius (cross-key rule)"},
      {"units:learns[].level = 99", "the level cap of the game (rules.progression.maxLevel) is lower"},
      {"units:promotions[].fromLevel = 99", "the level cap of the game (rules.progression.maxLevel) is lower"},
      {"units:fuelPerTurn = 999", "\"fuelPerTurn\" must be less than \"fuel\" (cross-key rule)"},
      {"units:items[]{item with uses}.uses = 999", "\"uses\" is at most the item's own uses (cross-file rule)"},
      {"items:light.shrink = 9", "\"shrink\" must be less than the radius (cross-key rule)"},
      {"skills:blocks[].if[]{turn}.turn{every n turns}.offset = 999", "\"offset\" must be less than \"every\" (cross-key rule)"},
      {"conditions:blocks[].if[]{turn}.turn{every n turns}.offset = 999", "\"offset\" must be less than \"every\" (cross-key rule)"},
  });
  const char* kEffect = "the blocks are the same shared definition as skills; every shape is tried there (a status condition is read by the same code)";
  s.insert({
      {"abilities:area.shape = \"burst\"", "the probe's area has a radius, which belongs to radius and cross only (cross-key rule); burst is tried by the area samples"},
      {"abilities:area.shape = \"line\"", "a line takes a length, not the probe's radius (cross-key rule); tried in the ability-area-line context"},
      {"abilities:target = \"self\"", "which targets an ability may have depends on its effect (cross-key rule); heal is tried with the others"},
      {"abilities:target = \"ally\"", "which targets an ability may have depends on its effect (cross-key rule)"},
      {"abilities:target = \"enemy\"", "which targets an ability may have depends on its effect (cross-key rule)"},
      {"abilities:target = \"cell\"", "which targets an ability may have depends on its effect (cross-key rule)"},
      {"abilities:effect{summon}.summon.level{fixed level} = 99", "the level cap of the game (rules.progression.maxLevel) is lower"},
      {"abilities:effect{summon}.summon.items[].minLevel = 99", "the level cap of the game (rules.progression.maxLevel) is lower"},
      {"abilities:effect{summon}.summon.items[].maxLevel = 99", "the level cap of the game (rules.progression.maxLevel) is lower"},
  });
  (void)kEffect;
}

}  // namespace conformance
