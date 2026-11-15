// Saves of structure kinds and research (docs/SAVES.md): a round trip in the middle of research, what the
// file looks like (and that games without these features write nothing new), and refusal of damaged
// fields one by one.
#include "json_edit.h"
#include "research_fixture.h"
#include "save_helpers.h"

using namespace ote;
using namespace restest;
using namespace jsonedit;

namespace {

// A battle with tools finished, the factory a foundry and masonry half researched (one turn left).
struct Mid {
  ResearchWorld w;
  bool ok() {
    return w.begin() && w.act(Action::research(Armor)) && w.act(Action::research(Tools)) &&
           w.act(Action::upgrade(w.structure(2, 0), Foundry)) && w.nextTurn() && w.act(Action::research(Masonry)) &&
           w.nextTurn();
  }
  bool fresh(Battle& b) {
    std::string err;
    return b.init(w.db, w.map, w.formulas, 1, err);
  }
  Json save() const {
    Json j;
    saveBattleState(w.battle, j);
    return j;
  }
};

std::string restoreError(Mid& m, const Json& saved, Battle* into = nullptr) {
  Battle local;
  Battle& r = into ? *into : local;
  std::string err;
  if (!m.fresh(r)) return "init failed";
  std::string before = savetest::stateText(r);
  bool ok = restoreBattleState(r, saved, err);
  if (!ok && savetest::stateText(r) != before) return "refused but changed the battle: " + err;  // must stay untouched
  return ok ? "" : err;
}

}  // namespace

TEST(research_save_round_trip_in_the_middle_of_research) {
  Mid m;
  CHECK(m.ok());
  CHECK_EQ(m.w.battle.researching(0), (int)Masonry);
  CHECK_EQ(m.w.battle.researchTurnsLeft(0), 1);
  Json saved = m.save();
  // Through text, like a file.
  Json reread;
  std::string err;
  CHECK(Json::parse(saved.dump(0), reread, err));
  Battle r;
  CHECK_STR(restoreError(m, reread, &r), "");
  CHECK_STR(savetest::stateText(r), savetest::stateText(m.w.battle));
  CHECK_EQ(r.researching(0), (int)Masonry);
  CHECK_EQ(r.researchTurnsLeft(0), 1);
  CHECK(r.hasTechnology(0, Tools) && r.hasTechnology(0, Armor) && !r.hasTechnology(0, Masonry));
  CHECK(!r.hasTechnology(1, Tools));
  CHECK_EQ(r.structures[(size_t)m.w.structure(2, 0)].type, (int)Foundry);
  CHECK_EQ(r.structures[(size_t)m.w.structure(2, 0)].mapType, m.w.db.findStructure("factory"));
  // Playing on gives the same game: the countdown ends on the next turn in both.
  std::vector<Event> a, b;
  for (int i = 0; i < 2; ++i) {
    CHECK(m.w.battle.apply(Action::endTurn(), a));
    CHECK(r.apply(Action::endTurn(), b));
  }
  CHECK(savetest::sameEvents(a, b));
  CHECK(r.hasTechnology(0, Masonry));
  CHECK_STR(savetest::stateText(r), savetest::stateText(m.w.battle));
}

TEST(research_save_writes_only_what_there_is_to_say) {
  Mid m;
  CHECK(m.ok());
  Json s = m.save();
  const Json& side0 = s["sides"][size_t(0)];
  CHECK(side0.has("research"));
  CHECK_EQ(side0["research"]["done"].size(), size_t(2));
  CHECK_EQ((int)side0["research"]["done"][size_t(0)].asNumber(), (int)Tools);
  CHECK_EQ((int)side0["research"]["done"][size_t(1)].asNumber(), (int)Armor);
  CHECK_EQ((int)side0["research"]["current"].asNumber(), (int)Masonry);
  CHECK_EQ((int)side0["research"]["left"].asNumber(), 1);
  CHECK(!s["sides"][size_t(1)].has("research"));  // red has done nothing
  // Only the upgraded structure carries its kind.
  int upgraded = m.w.structure(2, 0);
  for (size_t i = 0; i < s["structures"].size(); ++i) CHECK_EQ(s["structures"][i].has("type"), (int)i == upgraded);
  CHECK_EQ((int)s["structures"][(size_t)upgraded]["type"].asNumber(), (int)Foundry);
  // Nothing under way: only "done".
  m.w.battle.sides[0].researching = -1;
  Json t = m.save();
  CHECK(t["sides"][size_t(0)]["research"].has("done") && !t["sides"][size_t(0)]["research"].has("current"));
}

TEST(research_save_of_a_game_without_these_features_has_no_new_keys) {
  fixture::World w({"c.F.", "...."}, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 3, 1, false}});
  CHECK(w.begin());
  for (int i = 0; i < 3; ++i) w.act(Action::endTurn());
  Json j;
  saveBattleState(w.battle, j);
  std::string text = j.dump(0);
  CHECK(text.find("research") == std::string::npos);
  for (size_t i = 0; i < j["structures"].size(); ++i) CHECK(!j["structures"][i].has("type"));
}

TEST(research_save_with_a_game_that_has_research_but_nothing_done_writes_nothing_extra) {
  ResearchWorld w;
  CHECK(w.begin());
  Json j;
  saveBattleState(w.battle, j);
  CHECK(j.dump(0).find("research") == std::string::npos);
  for (size_t i = 0; i < j["structures"].size(); ++i) CHECK(!j["structures"][i].has("type"));
  Battle r;
  std::string err;
  CHECK(r.init(w.db, w.map, w.formulas, 1, err));
  CHECK(restoreBattleState(r, j, err));
}

TEST(research_save_a_chain_of_upgrades_is_restored) {
  ResearchWorld w;
  // keep: upgrade a second time through an extra step: hq -> keep -> (road is not reachable)
  w.db.structures[Keep].upgrades.push_back(UpgradeDef());
  w.db.structures[Keep].upgrades.back().to = w.db.findStructure("hq");  // back again: allowed, the kinds are reachable
  CHECK(w.begin());
  CHECK(w.act(Action::upgrade(w.structure(3, 0), Keep)));
  Json j;
  saveBattleState(w.battle, j);
  Battle r;
  std::string err;
  CHECK(r.init(w.db, w.map, w.formulas, 1, err));
  CHECK(restoreBattleState(r, j, err));
  CHECK_EQ(r.structures[(size_t)w.structure(3, 0)].type, (int)Keep);
}

namespace {

struct Damage {
  const char* what;
  std::vector<std::string> path;
  Json value;
  bool erase;
  const char* mention;  // the message must contain this
};

}  // namespace

TEST(research_save_refuses_every_kind_of_damaged_research_field) {
  Mid m;
  CHECK(m.ok());
  Json good = m.save();
  CHECK_STR(restoreError(m, good), "");
  const std::vector<std::string> r0 = {"sides", "#0", "research"};
  auto in = [&](const char* key) {
    std::vector<std::string> p = r0;
    p.push_back(key);
    return p;
  };
  Json obj = Json::makeObject();
  const Damage damage[] = {
      {"research is a number", r0, num(5), false, "sides[0].research"},
      {"research is a list", r0, Json::makeArray(), false, "sides[0].research"},
      {"done is missing", in("done"), Json(), true, "sides[0].research.done"},
      {"done is a number", in("done"), num(1), false, "sides[0].research.done"},
      {"done is an object", in("done"), obj, false, "sides[0].research.done"},
      {"done has a text entry", in("done"), [] { Json a = Json::makeArray(); a.push(Json::makeString("tools")); return a; }(), false, "done[0]"},
      {"done has a fraction", in("done"), list({0.5}), false, "done[0]"},
      {"done has a negative entry", in("done"), list({-1}), false, "done[0]"},
      {"done has an unknown technology", in("done"), list({99}), false, "done[0]"},
      {"done is not ascending", in("done"), list({2, 0}), false, "ascending"},
      {"done repeats a technology", in("done"), list({0, 0}), false, "ascending"},
      {"current is already finished", in("current"), num(0), false, "finished already"},
      {"current is unknown", in("current"), num(99), false, "sides[0].research.current"},
      {"current is negative", in("current"), num(-1), false, "sides[0].research.current"},
      {"current is a fraction", in("current"), num(1.5), false, "sides[0].research.current"},
      {"current is text", in("current"), str("masonry"), false, "sides[0].research.current"},
      {"current without left", in("left"), Json(), true, "go together"},
      {"left is zero", in("left"), num(0), false, "sides[0].research.left"},
      {"left is more than the technology takes", in("left"), num(3), false, "sides[0].research.left"},
      {"left is a fraction", in("left"), num(1.5), false, "sides[0].research.left"},
      {"left is text", in("left"), str("1"), false, "sides[0].research.left"},
      {"current whose prerequisite is not finished", in("done"), list({2}), false, "prerequisite"},
      {"left without current", in("current"), Json(), true, "go together"},
  };
  for (const Damage& d : damage) {
    Json bad = d.erase ? withoutAt(good, d.path) : withValue(good, d.path, d.value);
    std::string err = restoreError(m, bad);
    if (err.find(d.mention) == std::string::npos) {
      std::printf("  %s: expected a message with \"%s\", got \"%s\"\n", d.what, d.mention, err.c_str());
      CHECK(false);
    }
  }
}

TEST(research_save_refuses_a_finished_technology_without_its_prerequisite) {
  Mid m;
  CHECK(m.ok());
  Json bad = withoutAt(withoutAt(m.save(), {"sides", "#0", "research", "current"}), {"sides", "#0", "research", "left"});
  CHECK_STR(restoreError(m, bad), "");
  bad = withValue(bad, {"sides", "#0", "research", "done"}, list({(double)Masonry}));
  std::string err = restoreError(m, bad);
  CHECK(err.find("\"masonry\" is finished but its prerequisite \"tools\" is not") != std::string::npos);
}

TEST(research_save_refuses_research_in_a_game_that_has_none) {
  fixture::World w({"...."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false}});
  CHECK(w.begin());
  Json j;
  saveBattleState(w.battle, j);
  Json bad = withValue(j, {"sides", "#0", "research"}, Json::makeObject());
  Battle r;
  std::string err;
  CHECK(r.init(w.db, w.map, w.formulas, 1, err));
  CHECK(!restoreBattleState(r, bad, err));
  CHECK(err.find("the game has no research") != std::string::npos);
}

TEST(research_save_refuses_every_kind_of_damaged_structure_kind) {
  Mid m;
  CHECK(m.ok());
  Json good = m.save();
  int factory = m.w.structure(2, 0), city = m.w.structure(1, 0), hq = m.w.structure(3, 0), castle = m.w.structure(4, 0);
  auto at = [&](int s) { return std::vector<std::string>{"structures", "#" + std::to_string(s), "type"}; };
  const Damage damage[] = {
      {"type is text", at(factory), str("foundry"), false, "].type"},
      {"type is a fraction", at(factory), num(8.5), false, "type"},
      {"type is negative", at(factory), num(-1), false, "type"},
      {"type is past the last kind", at(factory), num(99), false, "type"},
      {"type has another footprint", at(factory), num((double)m.w.db.findStructure("castle")), false, "does not have the size"},
      {"type cannot be reached by upgrades", at(city), num(Foundry), false, "cannot be reached"},
      {"type of a structure with no upgrades", at(castle), num(Town), false, "type"},
      {"capture does not fit the saved kind", {"structures", "#" + std::to_string(hq), "capture"}, num(40), false, "capture"},
  };
  for (const Damage& d : damage) {
    Json bad = withValue(good, d.path, d.value);
    std::string err = restoreError(m, bad);
    if (err.empty() || err.find(d.mention) == std::string::npos) {
      std::printf("  %s: expected a message with \"%s\", got \"%s\"\n", d.what, d.mention, err.c_str());
      CHECK(false);
    }
  }
  // The same capture value is fine once the structure is the kind that has that many points.
  Json ok = withValue(withValue(good, at(hq), num(Keep)), {"structures", "#" + std::to_string(hq), "capture"}, num(40));
  CHECK_STR(restoreError(m, ok), "");
}

TEST(research_save_hit_points_count_the_research_bonus) {
  Mid m;
  CHECK(m.ok());
  Json good = m.save();
  int unit = 0;  // blue's soldier: maximum 10, with vigor 12
  std::vector<std::string> hp = {"units", "#" + std::to_string(unit), "hp"};
  CHECK(!restoreError(m, withValue(good, hp, num(12))).empty());  // above the maximum without vigor
  Json withVigor = withValue(good, {"sides", "#0", "research", "done"}, list({(double)Tools, (double)Armor, (double)Vigor}));
  CHECK_STR(restoreError(m, withValue(withVigor, hp, num(12))), "");
  CHECK(!restoreError(m, withValue(withVigor, hp, num(13))).empty());
}
