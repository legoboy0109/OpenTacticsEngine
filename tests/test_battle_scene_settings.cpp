// "Battle animations": the player setting against the game's default (docs/BATTLE_SCENE.md "Settings").
#include <cstring>

#include "options/effective.h"
#include "options/option_defs.h"
#include "test.h"

using namespace ote;

namespace {

bool resolves(const LookLayer& args, const LookLayer& perGame, const LookLayer& global, bool gamePresent, bool gameScene) {
  GameLook g;
  g.present = gamePresent;
  g.battleScene = gameScene;
  g.styleIds = {"default"};
  return resolveLook("assets", args, perGame, global, g).battleScene;
}

LookLayer layer(const char* v) {
  LookLayer l;
  l.battleScene = v;
  return l;
}

}  // namespace

TEST(battle_scene_setting_follows_the_game_default_when_unset) {
  CHECK(!resolves(LookLayer(), LookLayer(), LookLayer(), true, false));  // existing games: on the map
  CHECK(resolves(LookLayer(), LookLayer(), LookLayer(), true, true));    // the maker opted in
  CHECK(!resolves(LookLayer(), LookLayer(), LookLayer(), false, true));  // the launcher has no game
}

TEST(battle_scene_setting_player_choice_beats_the_game) {
  CHECK(resolves(LookLayer(), LookLayer(), layer("scene"), true, false));  // the player wants the scene in a map game
  CHECK(!resolves(LookLayer(), LookLayer(), layer("map"), true, true));    // ... or the map in a scene game
}

TEST(battle_scene_setting_priority_command_line_per_game_global) {
  CHECK(!resolves(layer("map"), layer("scene"), layer("scene"), true, true));   // command line first
  CHECK(resolves(LookLayer(), layer("scene"), layer("map"), true, false));      // then this game's override
  CHECK(!resolves(LookLayer(), layer("map"), layer("scene"), true, true));
  CHECK(resolves(LookLayer(), LookLayer(), layer("scene"), true, false));       // then the global setting
  CHECK(!resolves(layer("nonsense"), LookLayer(), LookLayer(), true, false));   // an unknown value is "unset"
  CHECK(resolves(layer("nonsense"), LookLayer(), LookLayer(), true, true));
}

TEST(battle_scene_option_row_and_settings_file) {
  const OptionDef* def = nullptr;
  for (const OptionDef& d : optionDefs())
    if (!strcmp(d.key, "battleScene")) def = &d;
  CHECK(def != nullptr);
  if (!def) return;
  CHECK(!strcmp(def->label, "Battle animations"));
  OptionContext ctx;
  std::vector<OptionChoice> global = def->choices(ctx);
  CHECK_EQ((int)global.size(), 3);
  CHECK(global[0].value.isNull() && global[0].label == "Game default");
  CHECK(global[1].label == "Scene" && global[1].value.asString() == "scene");
  CHECK(global[2].label == "On the map" && global[2].value.asString() == "map");
  GameEntry game;
  ctx.game = &game;
  CHECK(def->choices(ctx)[0].label == "Use global");  // the per-game screen follows the global setting
  // Stored in the settings file as the plain word; picked up as a layer.
  Settings s;
  s.setGlobal("battleScene", Json::makeString("scene"));
  s.setOverride("g", "battleScene", Json::makeString("map"));
  CHECK(globalLayer(s).battleScene == "scene");
  CHECK(overrideLayer(s, "g").battleScene == "map");
  CHECK(overrideLayer(s, "other").battleScene.empty());
}
