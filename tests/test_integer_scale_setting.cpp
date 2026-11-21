// "Whole-number scaling": the player setting (docs/GRAPHICS.md "Whole-number scaling"): default on, layering, the Options row, the file, the flag.
#include <cstring>

#include "args.h"
#include "options/effective.h"
#include "options/option_defs.h"
#include "test.h"

using namespace ote;

namespace {

bool resolves(const LookLayer& args, const LookLayer& perGame, const LookLayer& global) {
  GameLook g;
  g.styleIds = {"default"};
  return resolveLook("assets", args, perGame, global, g).integerScale;
}

LookLayer layer(int v) {
  LookLayer l;
  l.integerScale = v;
  return l;
}

}  // namespace

TEST(integer_scale_is_on_by_default) {
  CHECK(resolves(LookLayer(), LookLayer(), LookLayer()));
}

TEST(integer_scale_priority_command_line_per_game_global) {
  CHECK(!resolves(LookLayer(), LookLayer(), layer(0)));       // the global setting
  CHECK(resolves(LookLayer(), layer(1), layer(0)));           // this game's override beats it
  CHECK(!resolves(LookLayer(), layer(0), layer(1)));
  CHECK(!resolves(layer(0), layer(1), layer(1)));             // the command line beats both
  CHECK(resolves(layer(1), layer(0), layer(0)));
}

TEST(integer_scale_option_row_and_settings_file) {
  const OptionDef* def = nullptr;
  for (const OptionDef& d : optionDefs())
    if (!strcmp(d.key, "integerScale")) def = &d;
#ifdef OTE_NXDK
  CHECK(def == nullptr);  // the original Xbox has a fixed screen: no row
#else
  CHECK(def != nullptr);
  if (!def) return;
  CHECK(!strcmp(def->label, "Whole-number scaling"));
  OptionContext ctx;
  std::vector<OptionChoice> global = def->choices(ctx);  // On (nothing stored) / Off
  CHECK_EQ((int)global.size(), 2);
  CHECK(global[0].value.isNull() && global[0].label == "On");
  CHECK(global[1].label == "Off" && global[1].value.isBool() && !global[1].value.asBool(true));
  GameEntry game;
  ctx.game = &game;
  std::vector<OptionChoice> mine = def->choices(ctx);  // Use global / On / Off
  CHECK_EQ((int)mine.size(), 3);
  CHECK(mine[0].label == "Use global" && mine[0].value.isNull());
  CHECK(mine[1].label == "On" && mine[1].value.asBool(false));
  // Stored in the settings file as a plain true / false; picked up as a layer.
  Settings s;
  s.setGlobal("integerScale", Json::makeBool(false));
  s.setOverride("g", "integerScale", Json::makeBool(true));
  CHECK_EQ(globalLayer(s).integerScale, 0);
  CHECK_EQ(overrideLayer(s, "g").integerScale, 1);
  CHECK_EQ(overrideLayer(s, "other").integerScale, -1);
#endif
}

TEST(integer_scale_command_line_flag) {
  auto parse = [](const char* v) {
    const char* argv[] = {"ote-player", "--integer-scale", v};
    return parseArgs(3, const_cast<char**>(argv));
  };
  CHECK_EQ(parse("on").integerScale, 1);
  CHECK_EQ(parse("off").integerScale, 0);
  CHECK(!parse("maybe").error.empty());
  const char* none[] = {"ote-player"};
  CHECK_EQ(parseArgs(1, const_cast<char**>(none)).integerScale, -1);  // not given
}
