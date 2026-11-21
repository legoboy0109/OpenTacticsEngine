// Conformance tables, part 1b: the keys of game.json outside the mechanics of milestone 2 (those are in conformance_core.cpp).
#include "conformance_contexts.h"

namespace conformance {

void addGameContexts(std::vector<Context>& c) {
  const char* kStats = R"([{"id":"hp","name":"HP"},{"id":"mov","name":"Move"}])";
  // plain-economy: styles, economy, production, capture, structures; one map, one resource.
  c.push_back({"game-general", "plain-economy", {},
               {"game:formatVersion", "game:name", "game:description", "game:author", "game:saving", "game:battleScene",
                "game:battleBackgroundFrom", "game:battlePlatformFrom", "game:battleSceneSides", "game:battleSceneStaves",
                "game:menuUnavailable", "game:attackWeaponChoice", "game:view", "game:font", "game:stats", "game:roles",
                "game:moveTypes", "game:switches", "game:variables", "game:maps", "game:formulas",
                "game:styles", "game:rules.economy", "game:rules.production", "game:rules.capture", "game:rules.hitRoll",
                "game:rules.fog", "game:rules.display", "game:rules.promotion", "game:rules.places", "game:rules.supply",
                "game:rules.turnOrder{sides"},
               {{"game:stats", kStats},
                {"game:moveTypes", R"([{"id":"foot","name":"Foot"},{"id":"tread","name":"Tread"},{"id":"wheels","name":"Wheels"}])"},
                {"game:maps", R"([{"id":"twin_rivers","name":"Twin Rivers","file":"maps/twin_rivers.json"}])"},
                {"game:maps[].file", "\"maps/twin_rivers.json\""},
                {"game:styles[].background", "\"#102030\""},
                {"game:styles[].terrainColors", R"({"plain":"#102030"})"},
                {"game:styles[].structureColors", R"({"city":"#102030"})"}}});
  c.push_back({"game-scripts", "srpg-triangle", {}, {"game:scripts"}, {{"game:scripts", R"(["scripts/triangle.lua"])"}}});
  c.push_back({"game-grid", "hex-flat", {}, {"game:grid"}, {{"game:grid.type", "\"hex\""}, {"game:grid", R"({"type":"hex","layout":"flat"})"}}});
  c.push_back({"game-rescue", "srpg-carry", {}, {"game:rules.rescue"}, {{"game:rules.rescue.carrierStats", R"({"str":1})"}}});
  c.push_back({"game-rescue-pair", "srpg-pair", {}, {"game:rules.rescue.pairBonus"}, {{"game:rules.rescue.pairBonus", R"({"str":1})"}}});
  c.push_back({"game-items", "srpg-skills", {}, {"game:rules.items"}, {}});
  c.push_back({"game-difficulties", "plain-economy", {}, {"game:difficulties"},
               {{"game:difficulties.default", "\"normal\""}, {"game:difficulties.list", R"([{"id":"normal"}])"}}});
  c.push_back({"game-ai", "plain-economy", {}, {"game:rules.ai"}, {}});
  c.push_back({"game-progression-extras", "area-fire", {}, {"game:rules.progression.xpNoDamage", "game:rules.progression.computerGains"}, {}});
}

void addGameExtraSkips(std::map<QString, QString>& s) {
  s.insert({
      {"game:grid.type = \"square\"", "a hexagon fixture: its structures use rings, which a square game refuses (cross-file rule)"},
      {"game:rules.turnOrder{speed, custom}.mode = \"sides\"", "\"sides\" takes no other keys of the block (cross-key rule)"},
  });
}

}  // namespace conformance
