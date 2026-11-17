// Saving during the preparation and restoring into it (docs/SAVES.md "Preparation"); old saves still load.
#include "prep_fixture.h"

using namespace preptest;

namespace {

bool has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

bool restore(Run& r, const Json& file, Playthrough& pt, Battle& b, bool& hasBattle, std::string& err) {
  return restoreCampaignSave(r.g.db, r.g.formulas, "muster", file, pt, b, hasBattle, err);
}

}  // namespace

TEST(prep_a_save_made_during_the_screen_restores_into_it) {
  Run r;
  CHECK(r.begin("prep_save") && r.toChapter2() && r.openChapter2());
  std::string why;
  CHECK(r.bench("tomas") && r.deploy("bram", 1) && r.move("wren", 3));
  CHECK(prepGive(r.g.db, r.pt, r.mapDef, "sera", 0, "tomas", why));
  CHECK(prepBuy(r.g.db, r.pt, r.mapDef, "bram", r.g.db.findItem("potion"), why));
  Json file;
  std::string err;
  CHECK(buildCampaignSave(r.g.db, "muster", r.pt, nullptr, 1000, file, err));  // phase Battle, no battle yet
  Json again;  // through text, as a file is
  CHECK(Json::parse(file.dump(0), again, err));
  Playthrough back;
  Battle b;
  bool hasBattle = true;
  CHECK(restore(r, again, back, b, hasBattle, err));
  CHECK(!hasBattle);
  CHECK(back.prep.open && back.prep.folded);
  CHECK_EQ((int)back.prep.deployed.size(), 3);
  Json a, c;
  savePlaythrough(r.g.db, r.pt, a);
  savePlaythrough(r.g.db, back, c);
  CHECK_STR(a.dump(0), c.dump(0));  // exactly what it was: the choice, the trades, the purchase
  // The restored playthrough goes on: Begin, and the battle starts as chosen.
  r.pt = back;
  CHECK(prepBegin(r.g.db, r.pt, r.mapDef, why) && r.start());
  CHECK(r.unit("bram") >= 0 && r.unit("tomas") < 0);
}

TEST(prep_a_save_in_the_battle_restores_the_same_battle) {
  Run r;
  CHECK(r.begin("prep_save_battle") && r.toChapter2() && r.openChapter2());
  std::string why;
  CHECK(r.bench("tomas") && r.deploy("bram"));
  CHECK(prepBegin(r.g.db, r.pt, r.mapDef, why) && r.start());
  Json file;
  std::string err;
  CHECK(buildCampaignSave(r.g.db, "muster", r.pt, &r.b, 1000, file, err));
  Playthrough back;
  Battle b;
  bool hasBattle = false;
  CHECK(restore(r, file, back, b, hasBattle, err));
  CHECK(hasBattle);
  CHECK_EQ((int)b.units.size(), (int)r.b.units.size());
  for (size_t i = 0; i < b.units.size(); ++i) CHECK(b.units[i].storyId == r.b.units[i].storyId && b.units[i].cell == r.b.units[i].cell);
  CHECK_EQ(b.sides[0].resources[0], r.b.sides[0].resources[0]);
}

TEST(prep_old_saves_load_and_maps_without_it_save_as_before) {
  Run r;
  CHECK(r.begin("prep_save_old") && r.toChapter2());  // standing at chapter 2's scene: nothing of the screen exists yet
  Json file;
  std::string err;
  CHECK(buildCampaignSave(r.g.db, "muster", r.pt, nullptr, 1000, file, err));
  CHECK(!has(file.dump(0), "\"prep\"") && !has(file.dump(0), "lastDeployed"));
  Playthrough back;
  Battle b;
  bool hasBattle = false;
  CHECK(restore(r, file, back, b, hasBattle, err));
  CHECK(!back.prep.any() && back.lastDeployed.empty());
  // A chapter-1 battle save (no preparation on its map) has no new keys either.
  Run c;
  CHECK(c.begin("prep_save_old2") && c.before() && c.start());
  CHECK(buildCampaignSave(c.g.db, "muster", c.pt, &c.b, 1000, file, err));
  CHECK(!has(file.dump(0), "\"prep\"") && !has(file.dump(0), "lastDeployed"));
}

TEST(prep_damaged_preparation_in_a_save_is_refused_with_the_field) {
  Run r;
  CHECK(r.begin("prep_save_bad") && r.toChapter2() && r.openChapter2());
  Json file;
  std::string err;
  CHECK(buildCampaignSave(r.g.db, "muster", r.pt, nullptr, 1000, file, err));
  const std::string good = file.dump(0);
  Playthrough pt;
  Battle b;
  bool hasBattle = false;
  auto tryText = [&](const std::string& from, const std::string& to) {
    std::string text = good;
    size_t at = text.find(from);
    if (at == std::string::npos) std::printf("  \"%s\" not in the save\n", from.c_str());
    else text.replace(at, from.size(), to);
    Json j;
    std::string e;
    if (!Json::parse(text, j, e)) return std::string("PARSE: ") + e;
    e.clear();
    return restore(r, j, pt, b, hasBattle, e) ? std::string() : e;
  };
  CHECK_STR(tryText("\"id\":\"sera\",\"slot\":0", "\"id\":\"sera\",\"slot\":0"), "");
  CHECK(has(tryText("\"id\":\"sera\",\"slot\":0", "\"id\":\"sera\",\"slot\":99"), "playthrough.prep.deployed[0].slot"));
  CHECK(has(tryText("\"id\":\"sera\",\"slot\":0", "\"id\":\"zed\",\"slot\":0"), "\"zed\" cannot be deployed on this map"));
  CHECK(has(tryText("\"id\":\"tomas\",\"slot\":1", "\"id\":\"wren\",\"slot\":0"), "share a slot or a character"));
  // wren on the pond is fine, tomas on the pond is not: it cannot stand there.
  CHECK(has(tryText("\"id\":\"tomas\",\"slot\":1", "\"id\":\"tomas\",\"slot\":3"), "cannot stand on"));
  CHECK(has(tryText("\"open\":true", "\"open\":\"yes\""), "playthrough.prep.open"));
}
