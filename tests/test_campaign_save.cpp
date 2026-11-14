// Campaign saves (docs/SAVES.md "Campaign saves"): a round trip in every phase including in the middle of
// a battle, what the file looks like, and refusal of damaged files.
#include "campaign_run.h"

using namespace camptest;

namespace {

Json parse(const std::string& text) {
  Json j;
  std::string err;
  if (!Json::parse(text, j, err)) std::printf("  bad json: %s\n", err.c_str());
  return j;
}

// A save file made of the run as it stands (battle included when `withBattle`).
Json saveOf(Run& r, bool withBattle, long long at = 1000) {
  Json out;
  std::string err;
  if (!buildCampaignSave(r.g.db, "camp_save", r.pt, withBattle ? &r.b : nullptr, at, out, err)) std::printf("  build: %s\n", err.c_str());
  return parse(out.dump(0));  // through text, like a file
}

// Restores into fresh objects; returns the error ("" = fine).
std::string restore(Run& r, const Json& file, Playthrough& pt, Battle& b, bool& hasBattle) {
  std::string err;
  return restoreCampaignSave(r.g.db, r.g.formulas, "camp_save", file, pt, b, hasBattle, err) ? "" : err;
}

// Run at the start of the ford battle, a few AI steps in.
bool midBattle(Run& r, int steps = 6) {
  if (!r.begin("camp_save") || !r.startBattle()) return false;
  for (int i = 0; i < steps && !r.b.over; ++i) aiTakeStep(r.b, r.ev);
  return true;
}

// Replaces the value at a path of keys / "#index" steps in a copy.
Json setAt(const Json& j, const std::vector<std::string>& path, size_t at, const Json& v) {
  if (at == path.size()) return v;
  const std::string& step = path[at];
  if (j.isArray() && step[0] == '#') {
    Json out = Json::makeArray();
    size_t index = (size_t)std::atoi(step.c_str() + 1);
    for (size_t i = 0; i < j.size(); ++i) out.push(i == index ? setAt(j[i], path, at + 1, v) : j[i]);
    return out;
  }
  Json out = Json::makeObject();
  for (const Json::Member& m : j.members()) out.set(m.first, m.first == step ? setAt(m.second, path, at + 1, v) : m.second);
  if (!j.has(step.c_str())) out.set(step, setAt(Json(), path, at + 1, v));
  return out;
}
Json withValue(const Json& file, const std::vector<std::string>& path, const Json& v) { return setAt(file, path, 0, v); }

Json without(const Json& j, const std::string& key) {
  Json out = Json::makeObject();
  for (const Json::Member& m : j.members())
    if (m.first != key) out.set(m.first, m.second);
  return out;
}
Json num(double d) { return Json::makeNumber(d); }
Json str(const char* s) { return Json::makeString(s); }

}  // namespace

TEST(campaign_save_header_and_shape) {
  Run r;
  CHECK(midBattle(r));
  Json f = saveOf(r, true, 4242);
  CHECK(isCampaignSave(f));
  CHECK_STR(f["game"].asString(), "camp_save");
  CHECK_STR(f["gameName"].asString(), "Campaign Fixture");
  CHECK_STR(f["map"].asString(), "ford");
  CHECK_STR(f["mapName"].asString(), "The Ford");
  CHECK_STR(f["chapter"].asString(), "ch1");
  CHECK_STR(f["chapterName"].asString(), "The Ford");
  CHECK_EQ(f["saveVersion"].asInt(), 1);
  CHECK_EQ(f["savedAt"].asInt(), 4242);
  CHECK_EQ(f["round"].asInt(), r.b.round);
  CHECK(f["battle"].isObject() && f["playthrough"].isObject());
  CHECK_STR(f["playthrough"]["phase"].asString(), "battle");
  // Between the scene and the battle: no "battle", no round.
  Run s;
  CHECK(s.begin("camp_save") && s.scene(SceneKind::Before));
  Json g = saveOf(s, false);
  CHECK(!g.has("battle") && !g.has("round") && !g.has("activeSide"));
  CHECK_STR(g["playthrough"]["phase"].asString(), "battle");
  // A battle can only be saved in phase battle.
  Json out;
  std::string err;
  Run t;
  CHECK(t.begin("camp_save") && t.startBattle() && t.winAndFinish() == ChapterResult::Won);
  CHECK(!buildCampaignSave(t.g.db, "camp_save", t.pt, &t.b, 1, out, err));
  // No campaign: no campaign save.
  Game plain;
  CHECK(plain.load("camp_save_plain", ""));
  Playthrough none;
  CHECK(!buildCampaignSave(plain.db, "x", none, nullptr, 1, out, err));
  CHECK(!isCampaignSave(parse("{\"battle\":{}}")));
}

TEST(campaign_save_round_trip_in_every_phase) {
  Run r;
  CHECK(r.begin("camp_save"));
  auto roundTrip = [&](const char* label, bool withBattle) {
    Json f = saveOf(r, withBattle);
    Playthrough pt;
    Battle b;
    bool has = !withBattle;  // must be overwritten
    std::string err = restore(r, f, pt, b, has);
    if (!err.empty()) std::printf("  %s: %s\n", label, err.c_str());
    CHECK(err.empty());
    CHECK_EQ(has ? 1 : 0, withBattle ? 1 : 0);
    Run copy;
    copy.g.db = r.g.db;  // only used for the save helper below
    Json again;
    std::string e2;
    CHECK(buildCampaignSave(r.g.db, "camp_save", pt, has ? &b : nullptr, 1000, again, e2));
    CHECK_STR(again.dump(0), f.dump(0));  // the same file again, byte for byte
  };
  roundTrip("before", false);                              // fresh
  CHECK(r.scene(SceneKind::Before));
  roundTrip("battle, not started", false);                 // the scene done, no battle yet
  CHECK(r.startBattle());
  roundTrip("battle, new", true);
  for (int i = 0; i < 10; ++i) aiTakeStep(r.b, r.ev);
  roundTrip("battle, under way", true);
  playOut(r.b, r.ev);
  r.kill("lyra");
  r.decide(true);
  CHECK(finishChapterBattle(r.g.db, r.pt, r.b) == ChapterResult::Won);
  roundTrip("after", false);
  CHECK(r.scene(SceneKind::After));
  roundTrip("next chapter", false);
  // finished
  Run s;
  CHECK(s.begin("camp_save", R"({ "chapters": [ {"id":"a","map":"skirmish"} ] })") && s.startBattle());
  s.winAndFinish();
  s.scene(SceneKind::After);
  CHECK(s.pt.phase == Phase::Finished);
  Json f = saveOf(s, false);
  CHECK_STR(f["playthrough"]["phase"].asString(), "finished");
  Playthrough pt;
  Battle b;
  bool has = true;
  CHECK(restore(s, f, pt, b, has).empty() && !has && pt.phase == Phase::Finished);
  // over
  Run o;
  CHECK(o.begin("camp_save", R"({ "rules": {"onDefeat":"end"}, "chapters": [ {"id":"a","map":"skirmish"} ] })") && o.startBattle());
  o.decide(false);
  CHECK(finishChapterBattle(o.g.db, o.pt, o.b) == ChapterResult::Ended);
  Json g = saveOf(o, false);
  CHECK_STR(g["playthrough"]["phase"].asString(), "over");
  CHECK(restore(o, g, pt, b, has).empty() && pt.phase == Phase::Over);
}

TEST(campaign_save_mid_battle_restores_to_identical_play) {
  Run r;
  CHECK(midBattle(r, 9));
  Json f = saveOf(r, true);
  Playthrough pt;
  Battle b;
  bool has = false;
  CHECK(restore(r, f, pt, b, has).empty() && has);
  CHECK(b.roster != nullptr);  // the roster is back: later addUnit events dress units as well
  CHECK_STR(savetest::stateText(b), savetest::stateText(r.b));
  std::vector<Event> evA, evB;
  playOut(r.b, evA);
  playOut(b, evB);
  CHECK(savetest::sameEvents(evA, evB));
  CHECK_STR(savetest::stateText(b), savetest::stateText(r.b));
  CHECK(r.b.over);
  // Both finish into the same playthrough.
  ChapterResult ra = finishChapterBattle(r.g.db, r.pt, r.b), rb = finishChapterBattle(r.g.db, pt, b);
  CHECK(ra == rb);
  CHECK_STR(r.state(), [&] { Json j; savePlaythrough(r.g.db, pt, j); return j.dump(0); }());
}

TEST(campaign_save_restores_the_roster_for_a_chapter_after_the_first) {
  // The mill battle under way, with a roster that changed things: the restored battle must start from the
  // same roster as the original (units dressed), then the saved state goes on top.
  Run r;
  CHECK(r.begin("camp_save") && r.startBattle());
  r.b.units[(size_t)r.unit("eirik")].level = 4;
  r.winAndFinish();
  r.scene(SceneKind::After);
  CHECK(r.startBattle());
  CHECK(r.b.apply(Action::move(r.unit("eirik"), r.b.grid.at(2, 1)), r.ev));
  Json f = saveOf(r, true);
  Playthrough pt;
  Battle b;
  bool has = false;
  CHECK(restore(r, f, pt, b, has).empty() && has);
  CHECK_EQ(pt.chapter, 1);
  CHECK_EQ(b.units[(size_t)b.unitByStoryId("eirik")].level, 4);
  CHECK_EQ(b.units[(size_t)b.unitByStoryId("eirik")].cell, b.grid.at(2, 1));
  CHECK_STR(savetest::stateText(b), savetest::stateText(r.b));
}

TEST(campaign_save_usable_checks) {
  Run r;
  CHECK(midBattle(r));
  Json f = saveOf(r, true);
  std::string why;
  CHECK(campaignSaveUsable(r.g.db, "camp_save", f, why));
  CHECK(!campaignSaveUsable(r.g.db, "other_game", f, why) && why == "This save belongs to another game");
  CHECK(!campaignSaveUsable(r.g.db, "camp_save", withValue(f, {"saveVersion"}, num(2)), why) && why.find("different version") != std::string::npos);
  CHECK(!campaignSaveUsable(r.g.db, "camp_save", withValue(f, {"dataHash"}, str("0000000000000000")), why) &&
        why.find("different version") != std::string::npos);
  CHECK(!campaignSaveUsable(r.g.db, "camp_save", without(f, "playthrough"), why) && why == "The file is damaged");
  CHECK(!campaignSaveUsable(r.g.db, "camp_save", parse("[1]"), why));
  // A game that lost its campaign refuses the file.
  Game plain;
  CHECK(plain.load("camp_save_plain", ""));
  CHECK(!campaignSaveUsable(plain.db, "camp_save", f, why));
  // A rules change in ANY chapter's map makes the file "different": here the keep, chapters away from the save's.
  Run c;
  c.g.keepJson = mapJson(6, 3, sides("0", "[]"), heroesOnBlue(R"({"type":"grunt","side":"red","x":4,"y":1})"));
  CHECK(c.begin("camp_save"));
  CHECK(!campaignSaveUsable(c.g.db, "camp_save", f, why) && why.find("different version") != std::string::npos);
}

namespace {

struct Case {
  const char* name;
  std::vector<std::string> path;
  Json value;
  const char* mention;  // the message must name this
};

}  // namespace


namespace {

// The ford won, with items in the storage: a playthrough with a roster, carried sides and items to damage.
bool afterFord(Run& r) {
  if (!r.begin("camp_save") || !r.startBattle()) return false;
  playOut(r.b, r.ev);
  ItemStack potion;
  potion.item = r.g.db.findItem("potion");
  potion.uses = 3;
  r.b.sides[0].storage.push_back(potion);
  r.decide(true);
  return finishChapterBattle(r.g.db, r.pt, r.b) == ChapterResult::Won;
}

}  // namespace

TEST(campaign_save_damaged_playthrough_fields_are_refused) {
  Run r;
  CHECK(afterFord(r));
  CHECK(r.pt.roster.size() >= 2 && r.pt.roster[0].id == "eirik" && !r.pt.roster[0].items.empty());
  CHECK(!r.pt.sides.empty() && !r.pt.sides[0].storage.empty());
  Json f = saveOf(r, false);
  const std::string P = "playthrough";
  const Json big = num(1e12), frac = num(1.5), neg = num(-1), text = str("x");
  const Json yes = Json::makeBool(true);
  const Case cases[] = {
      {"playthrough not an object", {P}, num(3), "damaged"},
      {"chapter missing", {P, "chapter"}, Json(), "chapter"},
      {"chapter negative", {P, "chapter"}, neg, "chapter"},
      {"chapter past the end", {P, "chapter"}, num(5), "chapter"},
      {"chapter fraction", {P, "chapter"}, frac, "chapter"},
      {"chapter text", {P, "chapter"}, text, "chapter"},
      {"phase number", {P, "phase"}, num(1), "phase"},
      {"phase unknown", {P, "phase"}, str("dancing"), "phase"},
      {"roster not a list", {P, "roster"}, text, "roster"},
      {"roster entry not an object", {P, "roster", "#0"}, num(1), "roster[0]"},
      {"id not text", {P, "roster", "#0", "id"}, num(5), "roster[0].id"},
      {"id with a space", {P, "roster", "#0", "id"}, str("a b"), "roster[0].id"},
      {"id twice", {P, "roster", "#1", "id"}, str("eirik"), "roster[1].id"},
      {"type negative", {P, "roster", "#0", "type"}, neg, "roster[0].type"},
      {"type unknown", {P, "roster", "#0", "type"}, num(99), "roster[0].type"},
      {"name not text", {P, "roster", "#0", "name"}, num(1), "roster[0].name"},
      {"name too long", {P, "roster", "#0", "name"}, str(std::string(300, 'n').c_str()), "roster[0].name"},
      {"level zero", {P, "roster", "#0", "level"}, num(0), "roster[0].level"},
      {"level huge", {P, "roster", "#0", "level"}, big, "roster[0].level"},
      {"xp negative", {P, "roster", "#0", "xp"}, neg, "roster[0].xp"},
      {"xp fraction", {P, "roster", "#0", "xp"}, frac, "roster[0].xp"},
      {"stats not a list", {P, "roster", "#0", "stats"}, num(1), "roster[0].stats"},
      {"stats too short", {P, "roster", "#0", "stats"}, Json::makeArray(), "roster[0].stats"},
      {"stat text", {P, "roster", "#0", "stats", "#1"}, text, "roster[0].stats[1]"},
      {"stat huge", {P, "roster", "#0", "stats", "#1"}, big, "roster[0].stats[1]"},
      {"items not a list", {P, "roster", "#0", "items"}, num(1), "roster[0].items"},
      {"item not an object", {P, "roster", "#0", "items", "#0"}, num(1), "roster[0].items[0]"},
      {"item unknown", {P, "roster", "#0", "items", "#0", "item"}, num(99), "roster[0].items[0].item"},
      {"item uses too many", {P, "roster", "#0", "items", "#1", "uses"}, num(9), "roster[0].items[1].uses"},
      {"item uses zero", {P, "roster", "#0", "items", "#1", "uses"}, num(0), "roster[0].items[1].uses"},
      {"item drop text", {P, "roster", "#0", "items", "#0", "drop"}, text, "roster[0].items[0].drop"},
      {"side not an id", {P, "roster", "#0", "side"}, num(1), "roster[0].side"},
      {"gone text", {P, "roster", "#0", "gone"}, text, "roster[0].gone"},
      {"gone missing", {P, "roster", "#0", "gone"}, Json(), "roster[0].gone"},
      {"sides not a list", {P, "sides"}, num(1), "sides"},
      {"carried side not an object", {P, "sides", "#0"}, num(1), "sides[0]"},
      {"carried side id", {P, "sides", "#0", "id"}, num(1), "sides[0].id"},
      {"resources wrong length", {P, "sides", "#0", "resources"}, Json::makeArray(), "sides[0].resources"},
      {"resource text", {P, "sides", "#0", "resources", "#0"}, text, "sides[0].resources[0]"},
      {"resource huge", {P, "sides", "#0", "resources", "#0"}, big, "sides[0].resources[0]"},
      {"storage unknown item", {P, "sides", "#0", "storage", "#0", "item"}, num(77), "sides[0].storage[0].item"},
      {"switches wrong length", {P, "switches"}, Json::makeArray(), "switches"},
      {"switch not true/false", {P, "switches", "#0"}, num(1), "switches[0]"},
      {"switches missing", {P, "switches"}, Json(), "switches"},
      {"variables wrong length", {P, "variables"}, Json::makeArray(), "variables"},
      {"variable text", {P, "variables", "#0"}, text, "variables[0]"},
      {"variable too big", {P, "variables", "#0"}, num(2e9), "variables[0]"},
      {"variable fraction", {P, "variables", "#0"}, frac, "variables[0]"},
  };
  Playthrough pt = r.pt;
  Battle b;
  std::string before = r.state();
  int n = 0;
  for (const Case& c : cases) {
    Json bad = c.value.isNull() && c.path.size() > 1 ? withValue(f, c.path, Json()) : withValue(f, c.path, c.value);
    if (c.value.isNull()) {  // "missing": really remove the key
      std::vector<std::string> parent(c.path.begin(), c.path.end() - 1);
      Json holder = f;
      const Json* h = &holder;
      for (const std::string& step : parent) h = step[0] == '#' ? &(*h)[(size_t)std::atoi(step.c_str() + 1)] : &(*h)[step];
      bad = withValue(f, parent, without(*h, c.path.back()));
    }
    bool has = false;
    std::string err = restore(r, bad, pt, b, has);
    if (err.empty() || err.find(c.mention) == std::string::npos) {
      std::printf("  case \"%s\": error was \"%s\", wanted one naming %s\n", c.name, err.c_str(), c.mention);
      CHECK(false);
    }
    CHECK(!has);
    ++n;
  }
  CHECK(n > 40);
  Json j;
  savePlaythrough(r.g.db, pt, j);
  CHECK_STR(j.dump(0), before);  // nothing was applied by any refused file
  // And the undamaged file still restores.
  bool has = false;
  CHECK(restore(r, f, pt, b, has).empty());
}

TEST(campaign_save_damaged_battle_and_header_are_refused) {
  Run r;
  CHECK(midBattle(r));
  Json f = saveOf(r, true);
  Playthrough pt;
  Battle b;
  bool has = false;
  struct Bad {
    const char* name;
    Json file;
    const char* mention;
  };
  Json noBattle = without(f, "battle");
  Json phaseAfter = withValue(f, {"playthrough", "phase"}, str("after"));
  const Bad bad[] = {
      {"battle round zero", withValue(f, {"battle", "round"}, num(0)), "battle.round"},
      {"battle sides wrong", withValue(f, {"battle", "sides"}, Json::makeArray()), "battle.sides"},
      {"battle unit type", withValue(f, {"battle", "units", "#0", "type"}, num(99)), "units[0].type"},
      {"battle not an object", withValue(f, {"battle"}, num(1)), "battle"},
      {"battle in the wrong phase", phaseAfter, "battle"},
      {"a different version", withValue(f, {"saveVersion"}, num(9)), "different version"},
      {"another game", withValue(f, {"game"}, str("elsewhere")), "another game"},
      {"hash", withValue(f, {"dataHash"}, str("deadbeefdeadbeef")), "different version"},
      {"no playthrough", without(f, "playthrough"), "damaged"},
      {"playthrough is a list", withValue(f, {"playthrough"}, Json::makeArray()), "damaged"},
  };
  for (const Bad& c : bad) {
    std::string err = restore(r, c.file, pt, b, has);
    if (err.empty() || err.find(c.mention) == std::string::npos) {
      std::printf("  case \"%s\": error was \"%s\", wanted one naming %s\n", c.name, err.c_str(), c.mention);
      CHECK(false);
    }
  }
  CHECK(pt.roster.empty() && b.units.empty());  // nothing applied
  // A phase "battle" file without a battle is fine (the scene is done, the battle not started).
  CHECK(restore(r, noBattle, pt, b, has).empty() && !has && pt.phase == Phase::Battle);
}

TEST(campaign_save_survives_damage_everywhere) {
  // Crash finder: every value of the playthrough part replaced by other types, and the file cut off at many places.
  Run r;
  CHECK(afterFord(r));
  Json f = saveOf(r, false);
  std::vector<std::vector<std::string>> leaves;
  struct Walk {
    static void go(const Json& j, std::vector<std::string>& path, std::vector<std::vector<std::string>>& out) {
      if (j.isObject()) {
        for (const Json::Member& m : j.members()) {
          path.push_back(m.first);
          go(m.second, path, out);
          path.pop_back();
        }
      } else if (j.isArray()) {
        for (size_t i = 0; i < j.size(); ++i) {
          path.push_back("#" + std::to_string(i));
          go(j[i], path, out);
          path.pop_back();
        }
      } else {
        out.push_back(path);
      }
    }
  };
  std::vector<std::string> path;
  Walk::go(f["playthrough"], path, leaves);
  CHECK(leaves.size() > 40);
  const Json variants[] = {Json(), str("x"), num(-1), num(1e15), num(0.5), Json::makeBool(false), Json::makeArray(), Json::makeObject()};
  int tried = 0;
  for (const std::vector<std::string>& leaf : leaves)
    for (const Json& v : variants) {
      std::vector<std::string> full = {"playthrough"};
      full.insert(full.end(), leaf.begin(), leaf.end());
      Playthrough pt;
      Battle b;
      bool has = false;
      std::string err = restore(r, withValue(f, full, v), pt, b, has);  // ok or an error; never a crash
      if (err.empty()) {                                                  // if it was accepted it must be usable
        Json again;
        std::string e2;
        CHECK(buildCampaignSave(r.g.db, "camp_save", pt, nullptr, 1, again, e2));
      }
      ++tried;
    }
  CHECK(tried > 300);
  std::string text = f.dump(0);
  for (size_t cut = 0; cut < text.size(); cut += 7) {
    Json j;
    std::string perr;
    Playthrough pt;
    Battle b;
    bool has = false;
    if (Json::parse(text.substr(0, cut), j, perr)) restore(r, j, pt, b, has);
  }
}
