// Story side of the fidelity options: roster conditions in map events and scenes, and a unit taken off the map that can come back
// (docs/EVENTS.md "Campaign characters in map events", "Taking a unit off the map and bringing it back").
#include "campaign_run.h"
#include "story_fixture.h"

using namespace ote;
using namespace story;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

std::vector<std::string> field() { return {".........", ".........", "........."}; }

// Eirik (blue) and two red soldiers, gorm and hob.
struct Hide {
  StoryWorld w{field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}, {"soldier", 1, 8, 2, false}}, {"eirik", "gorm", "hob"}};
  int gorm() const { return w.unit("gorm"); }
  const Unit& g() const { return w.battle.units[(size_t)gorm()]; }
};

const char* kHideAtStart = R"([
  {"id":"hide","when":{"battle":"start"},"do":[{"removeUnit":{"unit":"gorm"},"hide":true}]},
  {"id":"probe","when":{"battle":"start"},"do":[
    {"if":[{"alive":"gorm"}],"then":[{"setSwitch":"alarm","to":true}]},
    {"if":[{"dead":"gorm"}],"then":[{"setSwitch":"saved","to":true}]},
    {"if":[{"rosterAlive":"gorm"}],"then":[{"setVariable":"kills","to":1}]},
    {"if":[{"rosterDead":"gorm"}],"then":[{"setVariable":"gold","to":1}]},
    {"if":[{"inRoster":"gorm"}],"then":[{"addVariable":"kills","by":10}]} ]}])";

}  // namespace

// ---- hide and show ----

TEST(hide_takes_a_unit_off_the_map_but_keeps_it) {
  Hide h;
  CHECK(h.w.play(kHideAtStart));
  const Unit& g = h.g();
  CHECK(!g.alive && g.hidden && g.cell == kNoCell);
  CHECK_EQ(g.hp, h.w.db.unitTypes[0].stats[0]);  // a removal sets 0; a hide keeps the hit points
  CHECK(!h.w.battle.exists(g));
  CHECK(h.w.count(EventType::UnitRemoved) == 1);
  for (int c = 0; c < h.w.battle.grid.cellCount(); ++c) CHECK(h.w.battle.occupant[(size_t)c] != g.id);
  // `alive` and `dead` read a hidden unit as neither; the roster conditions say it lives (a free battle: the map speaks).
  CHECK(!h.w.sw("alarm") && !h.w.sw("saved"));
  CHECK_EQ(h.w.var("kills"), 11);  // rosterAlive (1) and inRoster (+10); rosterDead did not fire
  CHECK_EQ(h.w.var("gold"), 0);
}

TEST(hide_a_hidden_unit_does_not_keep_its_side_in_the_fight) {
  Hide h;
  CHECK(h.w.play(kHideAtStart));
  Battle& b = h.w.battle;
  // hob is the last red unit on the map: defeating it defeats red although gorm is hidden somewhere.
  b.units[(size_t)h.w.unit("hob")].hp = 1;
  std::vector<Event> ev;
  Unit& hob = b.units[(size_t)h.w.unit("hob")];
  b.occupant[(size_t)hob.cell] = -1;
  b.occupant[(size_t)b.grid.at(1, 1)] = hob.id;
  hob.cell = b.grid.at(1, 1);
  CHECK(h.w.act(Action::attack(0, hob.id, 0)));
  CHECK(b.sides[1].defeated);
  CHECK(b.over);
  CHECK(h.g().hidden);  // still hidden: the side's defeat does not bring it back
}

TEST(hide_show_brings_the_same_unit_back_with_its_state) {
  Hide h;
  CHECK(h.w.play(R"([
    {"id":"hide","when":{"battle":"start"},"do":[{"removeUnit":{"unit":"gorm"},"hide":true}]},
    {"id":"back","when":{"turn":2},"do":[{"showUnit":"gorm","x":4,"y":1}]}])"));
  Unit& g = h.w.battle.units[(size_t)h.gorm()];
  g.hp = 4;
  g.level = 3;
  g.xp = 17;
  const size_t count = h.w.battle.units.size();
  CHECK(h.w.act(Action::endTurn()));
  CHECK(h.w.act(Action::endTurn()));
  CHECK(g.alive && !g.hidden && g.cell == h.w.battle.grid.at(4, 1));
  CHECK_EQ(g.hp, 4);
  CHECK_EQ(g.level, 3);
  CHECK_EQ(g.xp, 17);
  CHECK_EQ(h.w.battle.units.size(), count);  // the same unit, not a second one
  CHECK(h.w.battle.occupant[(size_t)g.cell] == g.id);
  CHECK(h.w.count(EventType::UnitAdded) == 1);
  CHECK(h.w.battle.exists(g));
}

TEST(hide_show_takes_the_nearest_free_square_and_refuses_nothing_to_show) {
  Hide h;
  CHECK(h.w.play(R"([
    {"id":"hide","when":{"battle":"start"},"do":[{"removeUnit":{"unit":"gorm"},"hide":true}]},
    {"id":"back","when":{"turn":2},"do":[{"showUnit":"gorm","x":0,"y":1},{"showUnit":"hob","x":0,"y":2}]}])"));
  CHECK(h.w.act(Action::endTurn()));
  CHECK(h.w.act(Action::endTurn()));
  // eirik stands on 0,1: gorm took a neighbouring free square. hob was never hidden: the command did nothing.
  CHECK(h.g().alive);
  CHECK_EQ(h.w.battle.grid.distance(h.g().cell, h.w.battle.grid.at(0, 1)), 1);
  CHECK(h.w.battle.units[(size_t)h.w.unit("hob")].cell == h.w.battle.grid.at(8, 2));
}

TEST(hide_add_unit_cannot_reuse_a_plain_units_id_but_show_unit_can) {
  // A plain unit id cannot be added again (docs/EVENTS.md): showUnit is how a hidden unit returns. (An addUnit of a hidden
  // *character* shows it too; that path is in Battle::storyAddUnit.)
  Hide h;
  CHECK(has(h.w.load(R"([{"id":"back","when":{"turn":2},"do":[{"addUnit":{"id":"gorm","type":"soldier","side":"red","x":5,"y":0}}]}])"), "already used"));
}

TEST(hide_removing_a_hidden_unit_for_good_makes_it_gone) {
  Hide h;
  CHECK(h.w.play(R"([
    {"id":"hide","when":{"battle":"start"},"do":[{"removeUnit":{"unit":"gorm"},"hide":true}]},
    {"id":"gone","when":{"turn":2},"do":[{"removeUnit":{"unit":"gorm"}},{"showUnit":"gorm","x":4,"y":1}]}])"));
  CHECK(h.w.act(Action::endTurn()));
  CHECK(h.w.act(Action::endTurn()));
  CHECK(!h.g().alive && !h.g().hidden && h.g().hp == 0);  // gone: the later showUnit found nothing hidden
}

TEST(hide_objectives_ignore_a_hidden_unit) {
  Hide h;
  // "Defeat gorm" is not met by hiding him; "defeat the red side" is met once the only visible red unit falls (hidden ones do not count).
  CHECK(h.w.play(R"([{"id":"hide","when":{"battle":"start"},"do":[{"removeUnit":{"unit":"gorm"},"hide":true}]}])",
                 R"({"text":"Get him","win":[{"defeated":{"unit":"gorm"}}]})"));
  CHECK(!h.w.battle.over);
}

TEST(hide_saves_keep_the_hidden_unit_and_only_it) {
  Hide h;
  CHECK(h.w.play(kHideAtStart));
  Json saved;
  saveBattleState(h.w.battle, saved);
  const std::string text = saved.dump(0);
  size_t count = 0;
  for (size_t at = text.find("\"hidden\""); at != std::string::npos; at = text.find("\"hidden\"", at + 1)) ++count;
  CHECK_EQ((int)count, 1);  // one key, on one unit
  Battle again;
  std::string err;
  CHECK(again.init(h.w.db, h.w.map, h.w.formulas, 1, err));
  CHECK(restoreBattleState(again, saved, err));
  CHECK(again.units[(size_t)h.gorm()].hidden && !again.units[(size_t)h.gorm()].alive);
  CHECK_EQ(again.units[(size_t)h.gorm()].hp, h.g().hp);
  Json second;
  saveBattleState(again, second);
  CHECK_STR(second.dump(0), text);
  // A damaged save: a hidden unit that stands on the map, or one with no hit points.
  for (const char* bad : {"\"alive\":true", "\"hp\":0"}) {
    std::string broken = text;
    const std::string unitFrom = std::string("\"hidden\":true");
    const size_t at = broken.find(unitFrom);
    CHECK(at != std::string::npos);
    const size_t from = broken.rfind('{', at);
    const size_t to = broken.find('}', at);
    std::string piece = broken.substr(from, to - from);
    const std::string key = std::string(bad).substr(0, std::string(bad).find(':'));
    const size_t k = piece.find(key);
    CHECK(k != std::string::npos);
    const size_t e = piece.find_first_of(",}", k);
    piece.replace(k, e == std::string::npos ? piece.size() - k : e - k, bad);
    broken.replace(from, to - from, piece);
    Json parsed;
    std::string perr;
    CHECK(Json::parse(broken, parsed, perr));
    Battle other;
    CHECK(other.init(h.w.db, h.w.map, h.w.formulas, 1, err));
    CHECK(!restoreBattleState(other, parsed, err));
  }
  // A battle that hides nobody saves without the key at all.
  Hide plain;
  CHECK(plain.w.play("[]"));
  Json untouched;
  saveBattleState(plain.w.battle, untouched);
  CHECK(untouched.dump(0).find("hidden") == std::string::npos);
}

TEST(hide_loader_refusals) {
  Hide h;
  CHECK(has(h.w.load(R"([{"id":"e","when":{"battle":"start"},"do":[{"removeUnit":{"unit":"gorm"},"hide":"yes"}]}])"), "hide"));
  CHECK(has(h.w.load(R"([{"id":"e","when":{"battle":"start"},"do":[{"removeUnit":{"unit":"gorm"},"vanish":true}]}])"), "unknown key"));
  CHECK(has(h.w.load(R"([{"id":"e","when":{"battle":"start"},"do":[{"showUnit":"nobody","x":1,"y":1}]}])"), "unknown unit"));
  CHECK(has(h.w.load(R"([{"id":"e","when":{"battle":"start"},"do":[{"showUnit":"gorm","x":99,"y":1}]}])"), "outside the map"));
  CHECK(has(h.w.load(R"([{"id":"e","when":{"battle":"start"},"do":[{"showUnit":"gorm"}]}])"), "\"x\""));
  CHECK(has(h.w.load(R"([{"id":"e","when":{"battle":"start"},"do":[{"showUnit":"gorm","x":1,"y":1,"walk":true}]}])"), "unknown key"));
  CHECK_STR(h.w.load(R"([{"id":"e","when":{"battle":"start"},"do":[{"showUnit":"gorm","x":1,"y":1}]}])"), "");
}

// ---- roster conditions ----

TEST(roster_condition_loader_and_refusals) {
  Hide h;
  const auto cond = [&](const char* c) { return h.w.load(std::string(R"([{"id":"e","when":{"battle":"start"},"if":[)") + c + R"(],"do":[]}])"); };
  CHECK_STR(cond(R"({"rosterAlive":"kara"})"), "");
  CHECK_STR(cond(R"({"rosterDead":"kara","party":"active"})"), "");
  CHECK_STR(cond(R"({"inRoster":"kara","party":"any"})"), "");
  CHECK(has(cond(R"({"rosterAlive":"kara","party":"north"})"), "\"party\" must be \"any\" or \"active\""));
  CHECK(has(cond(R"({"rosterAlive":7})"), "must be a character id"));
  CHECK(has(cond(R"({"rosterAlive":"not an id"})"), "must be a character id"));
  CHECK(has(cond(R"({"rosterAlive":"kara","side":"blue"})"), "unknown key"));
  CHECK(has(cond(R"({"rosterAlive":"kara","inRoster":"kara"})"), "has two conditions"));
  // also where conditions go in an `if` command and an objective's event
  CHECK_STR(h.w.load(R"([{"id":"e","when":{"battle":"start"},"do":[{"if":[{"inRoster":"kara"}],"then":[{"say":"hi"}]}]}])"), "");
}

namespace {

// A campaign run on the ford map with start events that probe the roster conditions.
const char* kProbe = R"([
  {"id":"any","when":{"battle":"start"},"if":[{"rosterAlive":"kara"}],"do":[{"setSwitch":"met_wanderer","to":true}]},
  {"id":"active","when":{"battle":"start"},"if":[{"rosterAlive":"kara","party":"active"}],"do":[{"setVariable":"kills","to":5}]},
  {"id":"dead","when":{"battle":"start"},"if":[{"rosterDead":"dax"}],"do":[{"setSwitch":"bridge_cut","to":true}]},
  {"id":"in","when":{"battle":"start"},"if":[{"inRoster":"dax"}],"do":[{"setVariable":"chapters","to":9}]} ])";

RosterUnit entry(const char* id, bool gone) {
  RosterUnit r;
  r.id = id;
  r.type = 0;
  r.name = id;
  r.level = 1;
  r.stats = {20, 6, 3, 4};
  r.gone = gone;
  r.side = "blue";
  return r;
}

}  // namespace

TEST(roster_conditions_read_the_campaign_roster_active_party_or_any) {
  using namespace camptest;
  Run r;
  r.g.fordJson = mapJson(6, 3, sides("100", "[]"), R"([ {"id":"eirik","type":"hero","side":"blue","x":0,"y":1}, {"type":"grunt","side":"red","x":5,"y":1} ])", kProbe);
  CHECK(r.begin("fid_roster", R"({ "name": "P", "parties": [ { "id": "main" }, { "id": "north" } ],
     "chapters": [ { "id": "ch1", "name": "One", "map": "ford" } ] })"));
  // kara is in the other (parked) party, dax is gone in the active one.
  ParkedParty north;
  north.id = "north";
  north.roster = {entry("kara", false)};
  r.pt.parked.push_back(north);
  r.pt.roster = {entry("dax", true)};
  CHECK(r.startBattle());
  const Database& db = r.g.db;
  const auto sw = [&](const char* id) { return r.b.storyState.switches[(size_t)db.findSwitch(id)] != 0; };
  const auto var = [&](const char* id) { return r.b.storyState.variables[(size_t)db.findVariable(id)]; };
  CHECK(sw("met_wanderer"));   // party any (the default): kara lives in the north party
  CHECK_EQ(var("kills"), 0);   // party active: she is not in this one
  CHECK(sw("bridge_cut"));     // dax fell
  CHECK_EQ(var("chapters"), 0);  // gone units are not "in the roster"
  // Now kara is in the active party's roster and dax is alive.
  Run s;
  s.g.fordJson = r.g.fordJson;
  CHECK(s.begin("fid_roster2", R"({ "name": "P", "parties": [ { "id": "main" }, { "id": "north" } ],
     "chapters": [ { "id": "ch1", "name": "One", "map": "ford" } ] })"));
  s.pt.roster = {entry("kara", false), entry("dax", false)};
  CHECK(s.startBattle());
  CHECK(s.b.storyState.switches[(size_t)s.g.db.findSwitch("met_wanderer")] != 0);
  CHECK_EQ(s.b.storyState.variables[(size_t)s.g.db.findVariable("kills")], 5);
  CHECK(s.b.storyState.switches[(size_t)s.g.db.findSwitch("bridge_cut")] == 0);
  CHECK_EQ(s.b.storyState.variables[(size_t)s.g.db.findVariable("chapters")], 9);
}

TEST(roster_conditions_the_map_unit_wins_over_the_roster) {
  using namespace camptest;
  Run r;
  // eirik is on the map: rosterAlive follows him (alive on the map), whatever the roster file says about a character that is not here.
  r.g.fordJson = mapJson(6, 3, sides("100", "[]"),
                         R"([ {"id":"eirik","type":"hero","side":"blue","x":0,"y":1}, {"id":"lyra","type":"ranger","side":"blue","x":0,"y":0}, {"type":"grunt","side":"red","x":5,"y":1} ])",
                         R"([ {"id":"live","when":{"battle":"start"},"if":[{"rosterAlive":"eirik"}],"do":[{"setSwitch":"met_wanderer","to":true}]},
                              {"id":"gone","when":{"turn":2},"do":[{"removeUnit":{"unit":"eirik"}}]},
                              {"id":"after","when":{"turn":3},"if":[{"rosterDead":"eirik"}],"do":[{"setSwitch":"bridge_cut","to":true}]} ])");
  CHECK(r.begin("fid_roster_map", R"({ "name": "P", "chapters": [ { "id": "ch1", "name": "One", "map": "ford" } ] })"));
  CHECK(r.startBattle());
  CHECK(r.b.storyState.switches[(size_t)r.g.db.findSwitch("met_wanderer")] != 0);
  CHECK(r.b.apply(Action::endTurn(), r.ev) && r.b.apply(Action::endTurn(), r.ev));  // round 2: eirik is removed
  CHECK(r.b.apply(Action::endTurn(), r.ev) && r.b.apply(Action::endTurn(), r.ev));  // round 3
  CHECK(r.b.storyState.switches[(size_t)r.g.db.findSwitch("bridge_cut")] != 0);
}

TEST(roster_conditions_in_a_scene_read_any_party_and_refuse_bad_forms) {
  using namespace camptest;
  Run r;
  const char* campaign = R"({ "name": "P", "parties": [ { "id": "main" }, { "id": "north" } ],
    "chapters": [ { "id": "ch1", "name": "One", "map": "ford",
      "before": [ {"if":[{"rosterAlive":"kara"}],"then":[{"say":"kara lives"}]},
                  {"if":[{"rosterAlive":"kara","party":"active"}],"then":[{"say":"in this party"}]},
                  {"if":[{"rosterDead":"dax"}],"then":[{"say":"dax fell"}]} ] } ] })";
  CHECK(r.begin("fid_roster_scene", campaign));
  ParkedParty north;
  north.id = "north";
  north.roster = {entry("kara", false)};
  r.pt.parked.push_back(north);
  r.pt.roster = {entry("dax", true)};
  CHECK(r.scene(SceneKind::Before));
  CHECK_STR(r.said(), "|kara lives;|dax fell;");  // kara is in a parked party only
  Game g;
  CHECK(has(g.loadError("fid_roster_scene_bad", R"({ "name": "P", "chapters": [ { "id": "ch1", "map": "ford",
      "before": [ {"if":[{"rosterAlive":"kara","party":"x"}],"then":[]} ] } ] })"), "\"party\" must be"));
  CHECK(has(g.loadError("fid_roster_scene_bad", R"({ "name": "P", "chapters": [ { "id": "ch1", "map": "ford",
      "before": [ {"if":[{"inRoster":"bad id"}],"then":[]} ] } ] })"), "character id"));
}

// ---- a hidden roster character returns alive ----

TEST(hide_roster_character_returns_to_the_roster_alive) {
  using namespace camptest;
  const auto play = [](const char* folder, const char* command) {
    auto run = std::make_unique<Run>();
    run->g.fordJson = mapJson(6, 3, sides("0", "[]"),
        R"([ {"id":"eirik","type":"hero","side":"blue","x":0,"y":1}, {"id":"lyra","type":"ranger","side":"blue","x":0,"y":0}, {"type":"grunt","side":"red","x":5,"y":1} ])",
        std::string(R"([ {"id":"away","when":{"battle":"start"},"do":[)") + command + "]} ]");
    CHECK(run->begin(folder, R"({ "name": "P", "rules": { "fallen": "gone" }, "chapters": [ { "id": "ch1", "name": "One", "map": "ford" } ] })"));
    CHECK(run->startBattle());
    return run;
  };
  auto hidden = play("fid_hide_roster", R"({"removeUnit":{"unit":"lyra"},"hide":true})");
  CHECK(hidden->winAndFinish() == ChapterResult::Won);
  const RosterUnit* lyra = hidden->pt.findUnit("lyra");
  CHECK(lyra && !lyra->gone);  // still with the company, alive
  auto removed = play("fid_hide_roster_gone", R"({"removeUnit":{"unit":"lyra"}})");
  CHECK(removed->winAndFinish() == ChapterResult::Won);
  const RosterUnit* lost = removed->pt.findUnit("lyra");
  CHECK(lost && lost->gone);  // a plain removal still counts as lost, as before
}
