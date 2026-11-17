// Campaign parties (docs/EVENTS.md "Parties"): the loader and its refusals, a switch to a party used for the first
// time (empty), a battle with it (a recruit joins it, loot and funds go to it), switching back (the first party is
// untouched), merging (units, storage, funds, research; the duplicate rule), saves at each stage, and saves of
// campaigns without parties staying as they were.
#include "campaign_run.h"

using namespace camptest;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

std::string replaceAll(std::string text, const std::string& from, const std::string& to) {
  for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size())) text.replace(at, from.size(), to);
  return text;
}

// The owner's flow in four short chapters: parts 1 and 2 (here ch1 and ch2's twin ch3) with `main`, part 3 with `north`, part 4 merges.
const char* kParties = R"({
  "name": "Parties", "rules": { "fallen": "gone", "onDefeat": "retry" },
  "parties": [ { "id": "main", "name": "The company" }, { "id": "north", "name": "The northern road" } ],
  "chapters": [
    { "id": "ch1", "name": "One", "map": "ford", "after": [ { "giveResource": { "gold": 25 }, "side": "blue" } ] },
    { "id": "ch2", "name": "Two", "map": "mill", "before": [ { "switchParty": "north" } ] },
    { "id": "ch3", "name": "Three", "map": "keep", "before": [ { "switchParty": "main" } ] },
    { "id": "ch4", "name": "Four", "map": "skirmish", "before": [ { "mergeParties": ["north"], "into": "main" } ] } ] })";

// Run with the north party's own map (ch2): the ford again but with kara in lyra's place, so the two parties differ.
struct PartyRun : Run {
  bool setup(const char* folder, const std::string& campaign = kParties) {
    g.millJson = replaceAll(fordMap(), "\"id\":\"lyra\"", "\"id\":\"kara\"");
    return begin(folder, campaign);
  }
  const CarriedSide* side(const std::vector<CarriedSide>& list, const char* id) const {
    for (const CarriedSide& s : list)
      if (s.id == id) return &s;
    return nullptr;
  }
  int gold(const std::vector<CarriedSide>& list) const {
    const CarriedSide* s = side(list, "blue");
    return s && !s->resources.empty() ? s->resources[0] : -1;
  }
  int stored(const std::vector<CarriedSide>& list, const char* item) const {
    const CarriedSide* s = side(list, "blue");
    int n = 0;
    const int index = g.db.findItem(item);
    if (s)
      for (const ItemGrant& g2 : s->storage) n += g2.item == index;
    return n;
  }
  bool ids(const Roster& r, const char* expected) const {
    std::string got;
    for (const RosterUnit& u : r) got += (got.empty() ? "" : " ") + u.id;
    return got == expected;
  }
  // Wins the chapter in play and runs its after scene.
  bool win() {
    if (winAndFinish() != ChapterResult::Won) return false;
    return scene(SceneKind::After);
  }
};

Json parse(const std::string& text) {
  Json j;
  std::string err;
  if (!Json::parse(text, j, err)) std::printf("  bad json: %s\n", err.c_str());
  return j;
}

// The playthrough through its JSON text and back: the same JSON again.
bool roundTrips(PartyRun& r) {
  Json out;
  savePlaythrough(r.g.db, r.pt, out);
  Playthrough back;
  std::string err;
  if (!restorePlaythrough(r.g.db, parse(out.dump(0)), back, err)) {
    std::printf("  restore: %s\n", err.c_str());
    return false;
  }
  Json again;
  savePlaythrough(r.g.db, back, again);
  return again.dump(0) == out.dump(0) && back.party == r.pt.party && back.parked.size() == r.pt.parked.size();
}

}  // namespace

// ---- the loader ----

TEST(parties_load_and_the_first_one_starts) {
  PartyRun r;
  CHECK(r.setup("parties_load"));
  CHECK_EQ((int)r.g.db.campaign->parties.size(), 2);
  CHECK(r.g.db.campaign->parties[1].id == "north" && r.g.db.campaign->parties[1].name == "The northern road");
  CHECK(r.pt.party == "main" && r.pt.parked.empty() && r.pt.merged.empty());
  CHECK_STR(activePartyName(r.g.db, r.pt), "The company");
  CHECK(r.g.db.campaign->chapters[1].before[0].kind == SceneCommand::Kind::SwitchParty);
  CHECK(r.g.db.campaign->chapters[3].before[0].kind == SceneCommand::Kind::MergeParties);
}

TEST(parties_a_campaign_without_them_has_none) {
  PartyRun r;
  CHECK(r.begin("parties_none"));
  CHECK(r.pt.party.empty() && activePartyName(r.g.db, r.pt).empty());
  Json out;
  savePlaythrough(r.g.db, r.pt, out);
  CHECK(!out.has("party") && !out.has("parties") && !out.has("mergedParties"));
}

TEST(parties_loader_refusals) {
  Game g;
  auto bad = [&](const char* parties, const char* chapter) {
    return g.loadError("parties_bad", std::string("{") + parties + R"("chapters":[{"id":"c","name":"C","map":"ford")" + chapter + "}]}");
  };
  const char* two = R"("parties":[{"id":"main"},{"id":"north"}],)";
  CHECK_STR(bad(two, ""), "");
  CHECK(has(bad(R"("parties":[{"id":"main"},{"id":"main"}],)", ""), "duplicate party id"));
  CHECK(has(bad(R"("parties":[{"id":"bad id"}],)", ""), "letters, digits"));
  CHECK(has(bad(R"("parties":[{"name":"x"}],)", ""), "id"));
  CHECK(has(bad(R"("parties":[{"id":"a","color":1}],)", ""), "unknown key"));
  CHECK(has(bad(R"("parties":"main",)", ""), "must be a list"));
  CHECK(has(bad(two, R"(,"before":[{"switchParty":"south"}])"), "unknown party \"south\""));
  CHECK(has(bad(two, R"(,"party":"south")"), "unknown party \"south\""));
  CHECK(has(bad("", R"(,"before":[{"switchParty":"north"}])"), "needs the campaign to have parties"));
  CHECK(has(bad("", R"(,"party":"north")"), "needs the campaign to have parties"));
  CHECK(has(bad(two, R"(,"before":[{"switchParty":"north","into":"main"}])"), "unknown key"));
  CHECK(has(bad(two, R"(,"before":[{"mergeParties":["north"]}])"), "missing required \"into\""));
  CHECK(has(bad(two, R"(,"before":[{"mergeParties":[],"into":"main"}])"), "list of party ids"));
  CHECK(has(bad(two, R"(,"before":[{"mergeParties":"north","into":"main"}])"), "list of party ids"));
  CHECK(has(bad(two, R"(,"before":[{"mergeParties":["main"],"into":"main"}])"), "the party it merges into"));
  CHECK(has(bad(two, R"(,"before":[{"mergeParties":["north","north"],"into":"main"}])"), "twice"));
  CHECK(has(bad(two, R"(,"before":[{"mergeParties":["west"],"into":"main"}])"), "unknown party \"west\""));
  CHECK(has(bad(two, R"(,"before":[{"mergeParties":["north"],"into":"east"}])"), "unknown party \"east\""));
  CHECK(has(bad(two, R"(,"before":[{"switchParty":"north"},{"mergeParties":["north"],"into":"main"}],"party":"main")"), ""));  // all fine together: no message
  // Inside an if and a choice answer too.
  CHECK(has(bad(two, R"(,"after":[{"if":[{"switch":"met_wanderer"}],"then":[{"switchParty":"nowhere"}]}])"), "unknown party"));
}

// ---- the flow ----

TEST(parties_a_party_used_for_the_first_time_starts_empty) {
  PartyRun r;
  CHECK(r.setup("parties_first") && r.startBattle() && r.win());
  CHECK(r.pt.party == "main" && r.pt.roster.size() >= 2);
  CHECK_EQ(r.gold(r.pt.sides), 125);  // the ford's 100 and the chapter's 25
  CHECK(r.pt.phase == Phase::Before && r.pt.chapter == 1);
  CHECK(r.scene(SceneKind::Before));  // ch2: switchParty north
  CHECK(r.pt.party == "north");
  CHECK(r.pt.roster.empty() && r.pt.sides.empty() && r.pt.lastDeployed.empty());  // a whole new game
  CHECK_EQ((int)r.pt.parked.size(), 1);
  CHECK(r.pt.parked[0].id == "main" && !r.pt.parked[0].roster.empty());
  CHECK_STR(activePartyName(r.g.db, r.pt), "The northern road");
  CHECK_EQ(r.pt.switches.size(), r.g.db.switches.size());
  CHECK(r.pt.switches[(size_t)r.g.db.findSwitch("met_wanderer")] == 1);  // the story is shared: the ford's switch is still set
}

TEST(parties_a_battle_with_the_new_party_goes_to_it_and_the_first_is_untouched) {
  PartyRun r;
  CHECK(r.setup("parties_battle") && r.startBattle() && r.win());
  Json before;
  savePlaythrough(r.g.db, r.pt, before);
  const std::string mainJson = before["roster"].dump(0) + before["sides"].dump(0);
  const int mainPotions = r.stored(r.pt.sides, "potion");
  CHECK(r.startBattle(2));  // ch2 (the north party's)
  CHECK_EQ(r.b.sides[0].resources[0], 100);  // the map's own gold only: main's 125 stay with main (the mill map gives 100, same as the ford)
  CHECK_EQ(r.b.sides[0].storage.size(), (size_t)1);  // only the map's potion, not main's storage
  CHECK(r.unit("eirik") >= 0 && r.unit("kara") >= 0);
  CHECK_EQ(r.b.units[(size_t)r.unit("eirik")].level, 1);  // fresh from the map's own definition: main's eirik had more
  // Loot: add gold during the battle by hand, as a won battle would carry it.
  r.b.sides[0].resources[0] += 7;
  CHECK(r.win());
  CHECK(r.pt.party == "north");
  CHECK_EQ(r.gold(r.pt.sides), 107);
  CHECK(r.pt.findUnit("wanderer") != nullptr && r.pt.findUnit("scout") != nullptr);  // the recruits joined the north party
  CHECK(r.pt.findUnit("kara") != nullptr && r.pt.findUnit("lyra") == nullptr);
  const Playthrough& pt = r.pt;
  CHECK(pt.parked.size() == 1 && pt.parked[0].id == "main");
  // Main's data, as saved: the same as before the north battle.
  Json after;
  savePlaythrough(r.g.db, r.pt, after);
  CHECK(after["parties"][0]["roster"].dump(0) + after["parties"][0]["sides"].dump(0) == mainJson);
  CHECK_EQ(r.gold(pt.parked[0].sides), 125);
  CHECK_EQ(r.stored(pt.parked[0].sides, "potion"), mainPotions);
}

TEST(parties_switching_back_restores_the_first_party_as_it_was) {
  PartyRun r;
  CHECK(r.setup("parties_back") && r.startBattle() && r.win());
  const Roster mainRoster = r.pt.roster;
  const std::vector<CarriedSide> mainSides = r.pt.sides;
  CHECK(r.startBattle(2) && r.win());
  CHECK(r.pt.party == "north");
  const Roster northRoster = r.pt.roster;
  CHECK(r.scene(SceneKind::Before));  // ch3: switchParty main
  CHECK(r.pt.party == "main");
  CHECK_EQ((int)r.pt.roster.size(), (int)mainRoster.size());
  for (size_t i = 0; i < mainRoster.size(); ++i) CHECK(r.pt.roster[i].id == mainRoster[i].id && r.pt.roster[i].level == mainRoster[i].level);
  CHECK_EQ(r.gold(r.pt.sides), r.gold(mainSides));
  CHECK_EQ((int)r.pt.parked.size(), 1);
  CHECK(r.pt.parked[0].id == "north" && r.pt.parked[0].roster.size() == northRoster.size());
  CHECK(r.startBattle(3));  // ch3's battle uses main's roster and storage
  CHECK_EQ(r.b.sides[0].resources[0], r.gold(mainSides));
  CHECK(r.unit("eirik") >= 0 && r.b.units[(size_t)r.unit("eirik")].level == mainRoster[0].level);
}

TEST(parties_a_switch_to_the_active_party_or_a_strange_one_changes_nothing) {
  PartyRun r;
  CHECK(r.setup("parties_noop") && r.startBattle() && r.win());
  const std::string state = r.state();
  CHECK(switchParty(r.g.db, r.pt, "main"));
  CHECK(!switchParty(r.g.db, r.pt, "nowhere"));
  CHECK_STR(r.state(), state);
}

TEST(parties_merging_in_the_flow_joins_the_parties_and_keeps_the_story) {
  PartyRun r;
  CHECK(r.setup("parties_merge") && r.startBattle() && r.win());
  CHECK(r.startBattle(2) && r.win());
  const size_t northUnits = r.pt.roster.size();
  CHECK(r.scene(SceneKind::Before));  // ch3: back to main
  CHECK(r.startBattle(3) && r.win());
  const size_t mainUnits = r.pt.roster.size();
  const int gold = r.gold(r.pt.sides) + r.gold(r.pt.parked[0].sides);
  const int switchesBefore = r.pt.switches[(size_t)r.g.db.findSwitch("met_wanderer")];
  CHECK(r.scene(SceneKind::Before));  // ch4: mergeParties north into main
  CHECK(r.pt.party == "main" && r.pt.parked.empty());
  CHECK(r.pt.merged.size() == 1 && r.pt.merged[0] == "north");
  std::set<std::string> seen;
  for (const RosterUnit& u : r.pt.roster) CHECK(seen.insert(u.id).second);  // nobody twice
  CHECK(r.pt.findUnit("kara") != nullptr);  // only the north party had her
  CHECK(r.pt.roster.size() > mainUnits && r.pt.roster.size() <= mainUnits + northUnits);
  CHECK_EQ(r.gold(r.pt.sides), gold);  // funds add up
  CHECK_EQ((int)r.pt.switches[(size_t)r.g.db.findSwitch("met_wanderer")], switchesBefore);
  CHECK(roundTrips(r));
  CHECK(r.startBattle(4));  // the merged party plays the last chapter
}

TEST(parties_merge_exact_numbers_and_the_duplicate_rule) {
  PartyRun r;
  r.g.researchJson = R"({ "technologies": [ { "id": "a", "name": "A", "cost": {}, "turns": 0 }, { "id": "b", "name": "B", "cost": {}, "turns": 0 },
                                            { "id": "c", "name": "C", "cost": {}, "turns": 0 } ] })";
  CHECK(r.setup("parties_merge_exact"));
  Database& db = r.g.db;
  const int potion = db.findItem("potion"), ring = db.findItem("ring"), tonic = db.findItem("tonic");
  auto grant = [&](int i) { return ItemGrant{i, db.items[(size_t)i].uses, false}; };
  auto unit = [&](const char* id, int level, std::vector<int> items) {
    RosterUnit u;
    u.id = id;
    u.type = 0;
    u.name = id;
    u.level = level;
    u.stats.assign(db.stats.size(), 1);
    u.side = "blue";
    for (int i : items) u.items.push_back(grant(i));
    return u;
  };
  // main: eirik (level 3, potion), lyra; gold 100, storage [potion]; research a b
  r.pt.roster = {unit("eirik", 3, {potion}), unit("lyra", 2, {})};
  CarriedSide& m = r.pt.sideCarry("blue", db.resources.size());
  m.resources[0] = 100;
  m.storage = {grant(potion)};
  m.research = {0, 1};
  r.pt.lastDeployed = {"eirik", "lyra"};
  CHECK(switchParty(db, r.pt, "north"));
  CHECK(r.pt.roster.empty() && r.pt.sides.empty());
  // north: eirik (level 9, ring + tonic: duplicate), kara; gold 30, storage [tonic]; research b c
  r.pt.roster = {unit("eirik", 9, {ring, tonic}), unit("kara", 5, {ring})};
  CarriedSide& n = r.pt.sideCarry("blue", db.resources.size());
  n.resources[0] = 30;
  n.storage = {grant(tonic)};
  n.research = {1, 2};
  r.pt.lastDeployed = {"kara", "eirik"};
  // Merge from north into main (main is not active: it is taken up first).
  mergeParties(db, r.pt, {"north"}, "main");
  CHECK(r.pt.party == "main" && r.pt.parked.empty());
  CHECK(r.ids(r.pt.roster, "eirik lyra kara"));
  CHECK_EQ(r.pt.findUnit("eirik")->level, 3);  // the target's copy stays
  CHECK_EQ((int)r.pt.findUnit("eirik")->items.size(), 1);
  CHECK_EQ(r.pt.findUnit("kara")->level, 5);
  CHECK_EQ(r.gold(r.pt.sides), 130);
  // storage: main's potion, north's tonic, then the other eirik's ring and tonic (the duplicate's items)
  const CarriedSide* s = r.side(r.pt.sides, "blue");
  CHECK(s && s->storage.size() == 4);
  CHECK_EQ(r.stored(r.pt.sides, "potion"), 1);
  CHECK_EQ(r.stored(r.pt.sides, "tonic"), 2);
  CHECK_EQ(r.stored(r.pt.sides, "ring"), 1);
  CHECK(s && (s->research == std::vector<int>{0, 1, 2}));
  CHECK((r.pt.lastDeployed == std::vector<std::string>{"eirik", "lyra", "kara"}));
  // The merged party is gone for good: switching to it, merging it again or merging into it does nothing.
  const std::string state = r.state();
  CHECK(!switchParty(db, r.pt, "north"));
  mergeParties(db, r.pt, {"north"}, "main");
  mergeParties(db, r.pt, {"main"}, "north");
  CHECK_STR(r.state(), state);
  CHECK(roundTrips(r));
}

TEST(parties_merging_a_party_never_used_only_retires_it_and_resources_stop_at_the_ceiling) {
  PartyRun r;
  CHECK(r.setup("parties_merge_unused"));
  mergeParties(r.g.db, r.pt, {"north"}, "main");
  CHECK(r.pt.party == "main" && r.pt.merged.size() == 1 && r.pt.roster.empty());
  PartyRun q;
  CHECK(q.setup("parties_ceiling"));
  q.pt.sideCarry("blue", 1).resources[0] = 900000000;
  CHECK(switchParty(q.g.db, q.pt, "north"));
  q.pt.sideCarry("blue", 1).resources[0] = 900000000;
  mergeParties(q.g.db, q.pt, {"north"}, "main");
  CHECK_EQ(q.gold(q.pt.sides), 1000000000);
  CHECK(roundTrips(q));
}

TEST(parties_a_chapters_party_key_switches_when_it_starts) {
  const char* campaign = R"({
    "name": "Chapter party", "rules": { "fallen": "gone", "onDefeat": "retry" },
    "parties": [ { "id": "main" }, { "id": "north" } ],
    "chapters": [
      { "id": "ch1", "name": "One", "map": "ford" },
      { "id": "ch2", "name": "Two", "map": "mill", "party": "north", "before": [ { "say": "North." } ] } ] })";
  PartyRun r;
  CHECK(r.setup("parties_chapter_key", campaign) && r.startBattle() && r.win());
  CHECK(r.pt.party == "main" && r.pt.phase == Phase::Before);
  CHECK(r.scene(SceneKind::Before));
  CHECK(r.pt.party == "north" && r.pt.roster.empty());
  CHECK(r.pt.phase == Phase::Battle);
}

TEST(parties_a_lost_battle_keeps_the_active_party) {
  PartyRun r;
  CHECK(r.setup("parties_retry") && r.startBattle() && r.win());
  CHECK(r.startBattle(2));
  const std::string state = r.state();
  r.decide(false);
  CHECK(finishChapterBattle(r.g.db, r.pt, r.b) == ChapterResult::Retry);
  CHECK(r.pt.party == "north");
  CHECK_STR(r.state(), state);
}

TEST(parties_a_scene_acts_on_the_active_party) {
  const char* campaign = R"({
    "name": "Scenes", "rules": { "fallen": "gone", "onDefeat": "retry" },
    "parties": [ { "id": "main" }, { "id": "north" } ],
    "chapters": [
      { "id": "ch1", "name": "One", "map": "ford",
        "after": [ { "switchParty": "north" },
                   { "if": [ { "alive": "eirik" } ], "then": [ { "say": "eirik is with the north." } ], "else": [ { "say": "eirik is not here." } ] },
                   { "giveResource": { "gold": 11 }, "side": "blue" } ] },
      { "id": "ch2", "name": "Two", "map": "mill" } ] })";
  PartyRun r;
  CHECK(r.setup("parties_scene", campaign) && r.startBattle());
  CHECK(r.winAndFinish() == ChapterResult::Won);
  CHECK(r.pt.findUnit("eirik") != nullptr);
  CHECK(r.scene(SceneKind::After));
  CHECK(has(r.said(), "eirik is not here."));  // the condition looks at the north party: empty
  CHECK(r.pt.party == "north");
  CHECK_EQ(r.gold(r.pt.sides), 11);  // giveResource went to the north party's side
  CHECK_EQ(r.gold(r.pt.parked[0].sides), 100);
}

// ---- saves ----

TEST(parties_saves_round_trip_at_each_stage) {
  PartyRun r;
  CHECK(r.setup("parties_saves"));
  CHECK(roundTrips(r));
  CHECK(r.startBattle() && r.win() && roundTrips(r));
  CHECK(r.scene(SceneKind::Before) && roundTrips(r));  // north active, main parked
  CHECK(r.startBattle(2));
  // A save in the middle of the battle: restores into the same battle with the active party's roster.
  for (int i = 0; i < 4 && !r.b.over; ++i) aiTakeStep(r.b, r.ev);
  Json file;
  std::string err;
  CHECK(buildCampaignSave(r.g.db, "camp_save", r.pt, &r.b, 1000, file, err));
  Playthrough pt;
  Battle b;
  bool hasBattle = false;
  CHECK(restoreCampaignSave(r.g.db, r.g.formulas, "camp_save", parse(file.dump(0)), pt, b, hasBattle, err));
  CHECK(hasBattle && pt.party == "north" && pt.parked.size() == 1 && pt.parked[0].id == "main");
  CHECK_EQ(b.round, r.b.round);
  r.decide(true);
  CHECK(finishChapterBattle(r.g.db, r.pt, r.b) == ChapterResult::Won);
  CHECK(roundTrips(r));
  CHECK(r.scene(SceneKind::After) && r.scene(SceneKind::Before) && r.pt.party == "main" && roundTrips(r));
  CHECK(r.startBattle(3) && r.win() && r.scene(SceneKind::Before) && r.pt.merged.size() == 1 && roundTrips(r));
  CHECK(r.pt.phase == Phase::Battle);
}

TEST(parties_the_save_keys_exist_only_with_parties_and_are_checked) {
  PartyRun r;
  CHECK(r.setup("parties_keys"));
  Json out;
  savePlaythrough(r.g.db, r.pt, out);
  CHECK(out.has("party") && out["party"].asString() == "main" && !out.has("parties") && !out.has("mergedParties"));
  CHECK(r.startBattle() && r.win() && r.scene(SceneKind::Before));
  savePlaythrough(r.g.db, r.pt, out);
  CHECK(out["party"].asString() == "north" && out["parties"].size() == 1 && out["parties"][0]["id"].asString() == "main");
  Playthrough back;
  std::string err;
  auto tryRestore = [&](const Json& j) { err.clear(); return restorePlaythrough(r.g.db, j, back, err) ? std::string() : err; };
  CHECK_STR(tryRestore(parse(out.dump(0))), "");
  auto with = [&](const char* key, const char* value) {
    Json j = parse(out.dump(0));
    j.set(key, parse(value));
    return tryRestore(j);
  };
  CHECK(has(with("party", "\"south\""), "playthrough.party: \"south\" is not one of the campaign's parties"));
  CHECK(has(with("party", "\"main\""), "the active party"));  // parked "main" is also the active one
  CHECK(has(with("mergedParties", "[\"north\"]"), "the active party or listed twice"));
  CHECK(has(with("mergedParties", "[\"main\"]"), "listed twice"));
  CHECK(has(with("parties", "[{\"id\":\"north\",\"roster\":[],\"sides\":[]}]"), "the active party"));
  CHECK(has(with("parties", "[{\"id\":\"main\",\"roster\":[{\"id\":\"x\"}],\"sides\":[]}]"), "playthrough.parties[0].roster[0]"));
  CHECK(has(with("parties", "[{\"id\":\"main\",\"sides\":[]}]"), "playthrough.parties[0].roster"));
  CHECK(has(with("parties", "[{\"id\":\"main\",\"roster\":[],\"sides\":[]},{\"id\":\"main\",\"roster\":[],\"sides\":[]}]"), "listed twice"));
  // A save without "party" plays the first party.
  Json plain = parse(out.dump(0));
  Json stripped = Json::makeObject();
  for (const Json::Member& m : plain.members())
    if (m.first != "party" && m.first != "parties") stripped.set(m.first, m.second);
  CHECK_STR(tryRestore(stripped), "");
  CHECK(back.party == "main" && back.parked.empty());
}

TEST(parties_campaigns_without_them_save_as_before_and_refuse_the_keys) {
  PartyRun r;
  CHECK(r.begin("parties_plain_save") && r.startBattle() && r.win());
  Json out;
  savePlaythrough(r.g.db, r.pt, out);
  for (const char* k : {"party", "parties", "mergedParties"}) CHECK(!out.has(k));
  Json j = parse(out.dump(0));
  j.set("party", Json::makeString("main"));
  Playthrough back;
  std::string err;
  CHECK(!restorePlaythrough(r.g.db, j, back, err) && has(err, "the campaign has no parties"));
}

TEST(parties_the_data_hash_covers_the_party_keys) {
  PartyRun r;
  CHECK(r.setup("parties_hash_a"));
  std::string a, b, err;
  CHECK(computeCampaignHash(r.g.db, a, err));
  Game q;
  CHECK(has(q.loadError("parties_hash_b", replaceAll(kParties, "\"id\": \"north\"", "\"id\": \"south\"")), "unknown party \"north\""));  // the chapters still name north
  PartyRun p;
  CHECK(p.setup("parties_hash_c", replaceAll(replaceAll(kParties, "\"north\"", "\"south\""), "The northern road", "The southern road")));
  CHECK(computeCampaignHash(p.g.db, b, err));
  CHECK(a != b);
  PartyRun n;  // a party's name is wording, like a chapter's: renaming it keeps saves valid
  CHECK(n.setup("parties_hash_d", replaceAll(kParties, "The northern road", "The far road")));
  std::string c;
  CHECK(computeCampaignHash(n.g.db, c, err));
  CHECK(a == c);
}
