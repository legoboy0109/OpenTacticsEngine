// Story scene presentation in the rules core and the loaders (docs/EVENTS.md "Scene presentation"): every command of a map
// event and of a campaign scene loads and is refused when wrong, runs in order as SceneEffect entries (the commands behind
// a fade run behind it), keeps the background and weather in the battle (saved only when set), never leaves the screen
// faded out at the end of a run of events, and a walked moveUnit is the same relocation as a plain one with a path.
#include <fstream>

#include "campaign_run.h"
#include "story_fixture.h"

using namespace story;

namespace {

const std::vector<std::string> kRows = {".........", ".........", "........."};
const std::vector<UnitSpec> kUnits = {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}};

void addBackgrounds(Database& db) {
  for (const char* id : {"forest", "hall"}) {
    SceneBackground bg;
    bg.id = id;
    bg.sheet = 0;
    db.graphics.backgrounds.push_back(bg);
  }
}

struct Scene : StoryWorld {
  Scene() : StoryWorld(kRows, kUnits, {"eirik", "gorm"}) { addBackgrounds(db); }
  // `commands` = the JSON list of one battle-start event.
  bool run(const std::string& commands) {
    return play(R"([{"id":"a","when":{"battle":"start"},"do":)" + commands + "}]");
  }
  std::string err(const std::string& commands) {
    return load(R"([{"id":"a","when":{"battle":"start"},"do":)" + commands + "}]");
  }
};

std::vector<Event> sceneEvents(const std::vector<Event>& events) {
  std::vector<Event> out;
  for (const Event& e : events)
    if (e.type == EventType::SceneEffect) out.push_back(e);
  return out;
}

bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

}  // namespace

TEST(scene_fx_commands_load_for_map_events) {
  Scene w;
  CHECK_STR(w.err(R"([{"background":"forest"}])"), std::string());
  CHECK_STR(w.err(R"([{"background":"hall","transition":"fade","ms":600}])"), std::string());
  CHECK_STR(w.err(R"([{"background":null}])"), std::string());
  CHECK_STR(w.err(R"([{"background":null,"transition":"fade"}])"), std::string());
  CHECK_STR(w.err(R"([{"fade":"out","ms":500,"color":"#102030"},{"fade":"in"}])"), std::string());
  CHECK_STR(w.err(R"([{"fade":"out","color":"#fff"}])"), std::string());
  CHECK_STR(w.err(R"([{"wait":0},{"wait":500}])"), std::string());
  CHECK_STR(w.err(R"([{"shake":{"ms":400,"strength":3}},{"shake":{}}])"), std::string());
  CHECK_STR(w.err(R"([{"leave":"eirik"},{"clearStage":true},{"leave":"Old Man"}])"), std::string());
  CHECK_STR(w.err(R"([{"weather":"rain"},{"weather":"snow"},{"weather":"none"}])"), std::string());
  CHECK_STR(w.err(R"([{"say":"Hi","speaker":"eirik","position":"left","auto":800},{"narrate":"Night.","auto":900},{"narrate":"Day."}])"), std::string());
  CHECK_STR(w.err(R"([{"focus":{"x":3,"y":1},"ms":0},{"focus":"eirik","ms":600,"wait":false},{"focus":"gorm","wait":true}])"), std::string());
  CHECK_STR(w.err(R"([{"moveUnit":"eirik","x":4,"y":1,"walk":true},{"moveUnit":"eirik","x":5,"y":1,"walk":true,"wait":false}])"), std::string());
  // The old modifiers still mean what they meant: "fade" of playMusic, "wait" has no meaning on a plain move.
  CHECK_STR(w.err(R"([{"playMusic":"boss","fade":1.0},{"stopMusic":true,"fade":0.5}])"), std::string());
}

TEST(scene_fx_commands_are_checked) {
  Scene w;
  CHECK(has(w.err(R"([{"background":"swamp"}])"), "unknown background \"swamp\""));
  CHECK(has(w.err(R"([{"background":3}])"), "\"background\" must name a background"));
  CHECK(has(w.err(R"([{"background":"forest","transition":"wipe"}])"), "\"transition\" must be \"cut\" or \"fade\""));
  CHECK(has(w.err(R"([{"background":"forest","transition":"fade","ms":10001}])"), "\"ms\" is milliseconds, between 0 and 10000"));
  CHECK(has(w.err(R"([{"background":"forest","ms":-1}])"), "\"ms\" is milliseconds"));
  CHECK(has(w.err(R"([{"background":"forest","speed":2}])"), "unknown key \"speed\""));
  CHECK(has(w.err(R"([{"fade":"sideways"}])"), "\"fade\" must be \"out\" or \"in\""));
  CHECK(has(w.err(R"([{"fade":"out","ms":20000}])"), "\"ms\" is milliseconds"));
  CHECK(has(w.err(R"([{"fade":"out","color":"black"}])"), "\"color\" must be a colour like \"#000000\""));
  CHECK(has(w.err(R"([{"fade":"out","color":"#12345"}])"), "\"color\" must be a colour"));
  CHECK(has(w.err(R"([{"wait":-5}])"), "\"wait\" is milliseconds"));
  CHECK(has(w.err(R"([{"wait":70000}])"), "\"wait\" is milliseconds"));
  CHECK(has(w.err(R"([{"wait":"long"}])"), "\"wait\" is milliseconds"));
  CHECK(has(w.err(R"([{"shake":{"ms":400,"strength":0}}])"), "\"strength\" is pixels"));
  CHECK(has(w.err(R"([{"shake":{"ms":400,"strength":99}}])"), "\"strength\" is pixels"));
  CHECK(has(w.err(R"([{"shake":{"ms":-1}}])"), "\"ms\" is milliseconds"));
  CHECK(has(w.err(R"([{"shake":3}])"), "\"shake\" must be {"));
  CHECK(has(w.err(R"([{"leave":""}])"), "\"leave\" must name the speaker"));
  CHECK(has(w.err(R"([{"clearStage":false}])"), "write { \"clearStage\": true }"));
  CHECK(has(w.err(R"([{"weather":"fog"}])"), "\"weather\" must be \"rain\", \"snow\" or \"none\""));
  CHECK(has(w.err(R"([{"say":"x","position":"middle"}])"), "\"position\" must be \"left\", \"right\" or \"center\""));
  CHECK(has(w.err(R"([{"say":"x","auto":0}])"), "\"auto\" is milliseconds, between 1 and 60000"));
  CHECK(has(w.err(R"([{"narrate":"x","speaker":"eirik"}])"), "unknown key \"speaker\""));
  CHECK(has(w.err(R"([{"narrate":5}])"), "\"narrate\" must be the text to show"));
  CHECK(has(w.err(R"([{"focus":"eirik","ms":-3}])"), "\"ms\" is milliseconds"));
  CHECK(has(w.err(R"([{"focus":"eirik","wait":"yes"}])"), "\"wait\" must be true or false"));
  CHECK(has(w.err(R"([{"moveUnit":"eirik","x":4,"y":1,"wait":false}])"), "\"wait\" only means something with \"walk\": true"));
  CHECK(has(w.err(R"([{"moveUnit":"eirik","x":4,"y":1,"walk":"yes"}])"), "\"walk\" must be true or false"));
  CHECK(has(w.err(R"([{"wait":5,"fade":"out"}])"), "has two commands"));
}

TEST(scene_fx_backgrounds_need_the_games_list) {
  StoryWorld w(kRows, kUnits, {"eirik", "gorm"});  // a game with no backgrounds at all
  CHECK(has(w.load(R"([{"id":"a","when":{"battle":"start"},"do":[{"background":"forest"}]}])"), "this game has no backgrounds"));
}

TEST(scene_fx_commands_run_in_order_behind_a_fade) {
  Scene w;
  // The classic: fade out, rearrange (a unit goes, the background changes), fade in. Entries come in command order.
  CHECK(w.run(R"([{"fade":"out","ms":300},{"background":"forest"},{"moveUnit":"eirik","x":2,"y":1},{"removeUnit":"gorm"},
                   {"wait":200},{"fade":"in","ms":300},{"say":"Back.","speaker":"eirik"}])"));
  std::vector<EventType> order;
  for (const Event& e : w.events)
    if (e.type == EventType::SceneEffect || e.type == EventType::UnitRelocated || e.type == EventType::UnitRemoved || e.type == EventType::Dialogue)
      order.push_back(e.type);
  CHECK(order == (std::vector<EventType>{EventType::SceneEffect, EventType::SceneEffect, EventType::UnitRelocated, EventType::UnitRemoved,
                                          EventType::SceneEffect, EventType::SceneEffect, EventType::Dialogue}));
  std::vector<Event> fx = sceneEvents(w.events);
  CHECK_EQ(fx.size(), size_t(4));
  if (fx.size() == 4) {
    CHECK((SceneFx::Kind)fx[0].a == SceneFx::Kind::Fade && fx[0].c == 300 && (fx[0].flags & kSceneOut));
    CHECK((SceneFx::Kind)fx[1].a == SceneFx::Kind::Background && w.battle.storyText(fx[1].b) == "forest" && !(fx[1].flags & kSceneFadeTrans));
    CHECK((SceneFx::Kind)fx[2].a == SceneFx::Kind::Wait && fx[2].c == 200);
    CHECK((SceneFx::Kind)fx[3].a == SceneFx::Kind::Fade && fx[3].c == 300 && !(fx[3].flags & kSceneOut));
  }
  // The rules happened at once, whatever the picture does.
  CHECK_EQ(w.battle.units[(size_t)w.unit("eirik")].cell, w.at(2, 1));
  CHECK(!w.battle.units[(size_t)w.unit("gorm")].alive);
  CHECK(w.battle.storyState.background == "forest");
  CHECK(!w.battle.storyState.faded);
}

TEST(scene_fx_a_run_that_ends_faded_out_fades_in_by_itself) {
  Scene w;
  CHECK(w.run(R"([{"fade":"out","ms":200,"color":"#102030"},{"background":"hall"},{"say":"Dark.","speaker":"eirik"}])"));
  std::vector<Event> fx = sceneEvents(w.events);
  CHECK_EQ(fx.size(), size_t(3));
  if (fx.size() == 3) {
    CHECK((fx[2].flags & kSceneAuto) && !(fx[2].flags & kSceneOut) && fx[2].d == 0x102030 && fx[2].c == kDefaultFadeMs);
  }
  CHECK(find(w.events, EventType::SceneEffect, (size_t)find(w.events, EventType::Dialogue)) > find(w.events, EventType::Dialogue));  // the automatic fade-in comes after the dialogue
  CHECK(!w.battle.storyState.faded);                        // so no save is ever made faded out
  // An explicit fade in leaves nothing to do.
  Scene v;
  CHECK(v.run(R"([{"fade":"out"},{"fade":"in"}])"));
  CHECK_EQ(sceneEvents(v.events).size(), size_t(2));
}

TEST(scene_fx_say_keys_ride_on_the_dialogue_entry) {
  Scene w;
  CHECK(w.run(R"([{"say":"Left.","speaker":"eirik","position":"left"},{"say":"Right.","speaker":"gorm","position":"right","auto":900},
                   {"narrate":"Night."},{"say":"Plain."}])"));
  std::vector<const Event*> d;
  for (const Event& e : w.events)
    if (e.type == EventType::Dialogue) d.push_back(&e);
  CHECK_EQ(d.size(), size_t(4));
  if (d.size() == 4) {
    CHECK_EQ(d[0]->flags & kDialoguePosMask, (int)kPosLeft);
    CHECK_EQ(d[1]->flags & kDialoguePosMask, (int)kPosRight);
    CHECK(d[1]->data.size() == 1 && d[1]->data[0] == 900);
    CHECK((d[2]->flags & kDialogueNarrate) && d[2]->a == -1);
    CHECK(d[3]->flags == 0 && d[3]->data.empty());  // a plain line is exactly what it always was
  }
}

TEST(scene_fx_focus_ms_and_wait_ride_on_the_focus_entry) {
  Scene w;
  CHECK(w.run(R"([{"focus":{"x":3,"y":1}},{"focus":{"x":4,"y":1},"ms":0},{"focus":"gorm","ms":800,"wait":false}])"));
  std::vector<const Event*> f;
  for (const Event& e : w.events)
    if (e.type == EventType::Focus) f.push_back(&e);
  CHECK_EQ(f.size(), size_t(3));
  if (f.size() == 3) {
    CHECK(f[0]->c == -1 && f[0]->flags == 0);  // untouched: the usual half second
    CHECK(f[1]->c == 0 && f[1]->flags == 0);   // a jump
    CHECK(f[2]->c == 800 && (f[2]->flags & kFocusNoWait) && f[2]->b == w.unit("gorm"));
  }
}

TEST(scene_fx_walked_move_is_the_same_relocation_with_a_path) {
  const char* walk = R"([{"id":"a","when":{"battle":"start"},"do":[{"moveUnit":"eirik","x":4,"y":1,"walk":true}]},
                         {"id":"r","when":{"reach":{"x":4,"y":1},"by":"eirik"},"do":[{"setSwitch":"saved","to":true}]}])";
  const char* plain = R"([{"id":"a","when":{"battle":"start"},"do":[{"moveUnit":"eirik","x":4,"y":1}]},
                          {"id":"r","when":{"reach":{"x":4,"y":1},"by":"eirik"},"do":[{"setSwitch":"saved","to":true}]}])";
  Scene a, b;
  CHECK(a.play(walk));
  CHECK(b.play(plain));
  // Same result, same triggers: the unit stands on the same square, the "reach" event ran, the stories agree.
  CHECK_EQ(a.battle.units[(size_t)a.unit("eirik")].cell, b.battle.units[(size_t)b.unit("eirik")].cell);
  CHECK(a.sw("saved") && b.sw("saved"));
  CHECK_STR(savetest::stateText(a.battle), savetest::stateText(b.battle));
  const Event* wm = nullptr;
  const Event* pm = nullptr;
  for (const Event& e : a.events) if (e.type == EventType::UnitRelocated) wm = &e;
  for (const Event& e : b.events) if (e.type == EventType::UnitRelocated) pm = &e;
  CHECK(wm && pm);
  if (wm && pm) {
    CHECK(pm->flags == 0 && pm->data.empty());  // no walk: the entry is what it was before
    CHECK(wm->a == pm->a && wm->b == pm->b && wm->c == pm->c);
    CHECK(wm->flags & kRelocateWalk);
    CHECK_EQ(wm->data.size(), size_t(5));  // (0,1) .. (4,1), one square at a time
    if (wm->data.size() == 5) {
      CHECK_EQ(wm->data.front(), wm->b);
      CHECK_EQ(wm->data.back(), wm->c);
      for (size_t i = 1; i < wm->data.size(); ++i) CHECK_EQ(a.battle.grid.distance(wm->data[i - 1], wm->data[i]), 1);
    }
  }
  // "wait": false rides on the entry too.
  Scene c;
  CHECK(c.play(R"([{"id":"a","when":{"battle":"start"},"do":[{"moveUnit":"eirik","x":3,"y":1,"walk":true,"wait":false}]}])"));
  const Event* cm = nullptr;
  for (const Event& e : c.events) if (e.type == EventType::UnitRelocated) cm = &e;
  CHECK(cm && (cm->flags & kRelocateWalk) && (cm->flags & kFocusNoWait));
}

TEST(scene_fx_walk_goes_round_what_cannot_be_crossed_and_ignores_units) {
  // A wall of water the foot unit cannot enter, with a gap at the bottom row: the walk goes through the gap.
  const std::vector<std::string> rows = {"...#.....", "...#.....", "........."};
  Scene v;
  v.map = fixture::makeMap(v.db, rows, kUnits);
  v.map.units[0].id = "eirik";
  v.map.units[1].id = "gorm";
  CHECK(v.play(R"([{"id":"a","when":{"battle":"start"},"do":[{"moveUnit":"eirik","x":5,"y":0,"walk":true}]}])"));
  const Event* m = nullptr;
  for (const Event& e : v.events) if (e.type == EventType::UnitRelocated) m = &e;
  CHECK(m != nullptr);
  if (m) {
    CHECK(m->flags & kRelocateWalk);
    for (int cell : m->data) CHECK(v.battle.grid.x((Cell)cell) != 3 || v.battle.grid.y((Cell)cell) == 2);
  }
}

TEST(scene_fx_background_and_weather_are_saved_only_when_set_and_come_back) {
  Scene q;
  CHECK(q.run(R"([{"setSwitch":"saved","to":true}])"));
  CHECK(savetest::stateText(q.battle).find("\"scene\"") == std::string::npos);  // every other save stays as it was

  Scene w;
  CHECK(w.run(R"([{"background":"hall"},{"weather":"rain"},{"fade":"out"},{"fade":"in"}])"));
  CHECK(w.battle.storyState.background == "hall" && w.battle.storyState.weather == "rain");
  Json saved;
  saveBattleState(w.battle, saved);
  CHECK(saved["story"].has("scene"));
  CHECK_STR(saved["story"]["scene"]["background"].asString(), std::string("hall"));
  CHECK_STR(saved["story"]["scene"]["weather"].asString(), std::string("rain"));
  Battle r;
  std::string err;
  CHECK(r.init(w.db, w.map, w.formulas, 1, err));
  CHECK(restoreBattleState(r, saved, err));
  CHECK(r.storyState.background == "hall" && r.storyState.weather == "rain" && !r.storyState.faded);
  CHECK_STR(savetest::stateText(r), savetest::stateText(w.battle));

  // Taking them away leaves the save as if never set.
  Scene u;
  CHECK(u.run(R"([{"background":"hall"},{"weather":"snow"},{"background":null},{"weather":"none"}])"));
  CHECK(savetest::stateText(u.battle).find("\"scene\"") == std::string::npos);

  // Damaged values are refused, and a background the game does not have.
  Json bad = saved;
  Json story = bad["story"];
  Json scene = story["scene"];
  scene.set("weather", Json::makeString("fog"));
  story.set("scene", scene);
  bad.set("story", story);
  Battle r2;
  CHECK(r2.init(w.db, w.map, w.formulas, 1, err));
  CHECK(!restoreBattleState(r2, bad, err) && has(err, "battle.story.scene.weather"));
  scene.set("weather", Json::makeString("rain"));
  scene.set("background", Json::makeString("swamp"));
  story.set("scene", scene);
  bad.set("story", story);
  Battle r3;
  CHECK(r3.init(w.db, w.map, w.formulas, 1, err));
  CHECK(!restoreBattleState(r3, bad, err) && has(err, "battle.story.scene.background"));
}

TEST(scene_fx_commands_count_in_the_hash_but_the_picture_does_not) {
  // Which picture / weather / narrator wording is presentation; whether a command is there is part of the story.
  using namespace camptest;
  Game g;
  CHECK(g.load("scene_fx_hash"));
  std::string ford = fordMap();
  auto hashWith = [&](const char* extra) {
    std::string m = ford;
    if (extra[0]) {
      size_t at = m.find("{\"changeSide\":\"wanderer\"");
      m.insert(at, std::string(extra) + ",");
    }
    Game::write(g.dir + "/maps/ford.json", m);
    Database db;
    std::string err, h;
    if (!loadGame(g.dir, db, err) || !computeDataHash(db, "ford", h, err)) return "error: " + err;
    return h;
  };
  // (No graphics here: a background command would be refused, so the weather and the narrator carry the test.)
  std::string none = hashWith(""), rain = hashWith("{\"weather\":\"rain\"}"), snow = hashWith("{\"weather\":\"snow\"}");
  std::string n1 = hashWith("{\"narrate\":\"Night.\"}"), n2 = hashWith("{\"narrate\":\"Day.\"}");
  CHECK(none.rfind("error", 0) != 0 && rain.rfind("error", 0) != 0);
  CHECK_STR(rain, snow);
  CHECK(rain != none);
  CHECK_STR(n1, n2);
  CHECK(n1 != none);
}

// ---- campaign scenes ----

namespace {

bool copyBytes(const std::string& from, const std::string& to) {
  std::ifstream in(from, std::ios::binary);
  std::ofstream out(to, std::ios::binary);
  out << in.rdbuf();
  return (bool)in && (bool)out;
}

// The campaign fixture game plus the scene-pres art and graphics.json, with `campaign` as data/campaign.json.
bool loadWithArt(camptest::Game& g, const char* folder, const std::string& campaign, std::string* err = nullptr) {
  using namespace camptest;
  removeFile(savetest::tempFolder(folder) + "/data/graphics.json");  // (a leftover of an earlier run must not stop the load)
  if (!g.load(folder, "", nullptr)) return false;  // (the game without a campaign first: the scene art is written next)
  makeDir(g.dir + "/art");
  const std::string art = ote_test::sourceDir() + "/tests/fixtures/scene-pres/";
  bool ok = true;
  for (const char* f : {"bg_forest.png", "bg_places.png", "faces.png"}) ok = ok && copyBytes(art + "art/" + f, g.dir + "/art/" + f);
  ok = ok && Game::write(g.dir + "/data/graphics.json", [&] {
         std::ifstream in(art + "data/graphics.json");
         return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
       }());
  ok = ok && Game::write(g.dir + "/data/campaign.json", campaign);
  g.db = Database();
  std::string e;
  ok = ok && loadGame(g.dir, g.db, e) && g.formulas.init(g.db, e);
  if (!ok && err) *err = e;
  else if (!ok) std::printf("  load: %s\n", e.c_str());
  return ok;
}

}  // namespace

TEST(scene_fx_campaign_scene_commands_become_lines_in_order) {
  using namespace camptest;
  Run r;
  const std::string campaign = R"({ "chapters": [ { "id": "ch1", "name": "A", "map": "ford", "before": [
    {"fade":"out","ms":200,"color":"#102040"}, {"background":"forest"}, {"fade":"in","ms":150},
    {"say":"Left.","speakerName":"Eirik","portrait":"eirik","position":"left"},
    {"wait":300}, {"shake":{"ms":200,"strength":4}}, {"narrate":"Night."},
    {"background":"hall","transition":"fade","ms":250}, {"leave":"Eirik"}, {"say":"Auto.","speakerName":"Lyra","auto":700},
    {"clearStage":true}, {"background":null}, {"fade":"out"} ] } ] })";
  CHECK(loadWithArt(r.g, "scene_fx_campaign", campaign));
  CHECK(r.pt.begin(r.g.db));
  CHECK(r.scene(SceneKind::Before));
  using K = SceneLine::Kind;
  using F = SceneFx::Kind;
  CHECK_EQ(r.lines.size(), size_t(13));
  if (r.lines.size() == 13) {
    CHECK(r.lines[0].kind == K::Fx && r.lines[0].fx == F::Fade && r.lines[0].out && r.lines[0].fadeMs == 200 && r.lines[0].color == 0x102040);
    CHECK(r.lines[1].kind == K::Fx && r.lines[1].fx == F::Background && r.lines[1].text == "forest" && !r.lines[1].fadeTransition);
    CHECK(r.lines[2].kind == K::Fx && r.lines[2].fx == F::Fade && !r.lines[2].out && r.lines[2].fadeMs == 150);
    CHECK(r.lines[3].kind == K::Say && r.lines[3].position == kPosLeft && r.lines[3].portrait == "eirik");
    CHECK(r.lines[4].fx == F::Wait && r.lines[4].fadeMs == 300);
    CHECK(r.lines[5].fx == F::Shake && r.lines[5].fadeMs == 200 && r.lines[5].strength == 4);
    CHECK(r.lines[6].kind == K::Say && r.lines[6].narrate && r.lines[6].speaker.empty() && r.lines[6].text == "Night.");
    CHECK(r.lines[7].fx == F::Background && r.lines[7].text == "hall" && r.lines[7].fadeTransition && r.lines[7].fadeMs == 250);
    CHECK(r.lines[8].fx == F::Leave && r.lines[8].text == "Eirik");
    CHECK(r.lines[9].kind == K::Say && r.lines[9].autoMs == 700 && r.lines[9].position == 0);
    CHECK(r.lines[10].fx == F::Leave && r.lines[10].text.empty());
    CHECK(r.lines[11].fx == F::Background && r.lines[11].text.empty());
    CHECK(r.lines[12].fx == F::Fade && r.lines[12].out);
  }
  CHECK(r.pt.phase == Phase::Battle);  // presentation changes nothing in the playthrough
}

TEST(scene_fx_campaign_scene_commands_are_checked) {
  using namespace camptest;
  Game g;
  auto err = [&](const std::string& cmds) {
    std::string e;
    loadWithArt(g, "scene_fx_campaign_bad", R"({ "chapters": [ { "id": "ch1", "name": "A", "map": "ford", "before": )" + cmds + " } ] }", &e);
    return e;
  };
  CHECK(err(R"([{"background":"swamp"}])").find("unknown background \"swamp\"") != std::string::npos);
  CHECK(err(R"([{"weather":"rain"}])").find("\"weather\" is for map events only") != std::string::npos);
  CHECK(err(R"([{"focus":{"x":1,"y":1}}])").find("is not a scene command") != std::string::npos);
  CHECK(err(R"([{"moveUnit":"eirik","x":1,"y":1,"walk":true}])").find("is not a scene command") != std::string::npos);
  CHECK(err(R"([{"wait":-1}])").find("\"wait\" is milliseconds") != std::string::npos);
  CHECK(err(R"([{"fade":"out","ms":99999}])").find("\"ms\" is milliseconds") != std::string::npos);
  CHECK(err(R"([{"say":"x","position":"up"}])").find("\"position\" must be") != std::string::npos);
  CHECK(err(R"([{"shake":{"strength":50}}])").find("\"strength\" is pixels") != std::string::npos);
  // The old scene commands keep their modifiers.
  CHECK_STR(err(R"([{"playMusic":"story","fade":1.5},{"stopMusic":true,"fade":0.5},{"say":"Hi"}])"), std::string());
  CHECK_STR(err(R"([{"fade":"in"},{"wait":0},{"background":"forest","transition":"fade"},{"background":null},{"leave":"x"},{"clearStage":true}])"), std::string());
}

TEST(scene_fx_graphics_backgrounds_load_and_are_checked) {
  using namespace camptest;
  Game g;
  CHECK(loadWithArt(g, "scene_fx_graphics", kCampaignJson));
  CHECK_EQ(g.db.graphics.backgrounds.size(), size_t(3));
  const int forest = g.db.graphics.findBackground("forest"), hall = g.db.graphics.findBackground("hall");
  CHECK(forest >= 0 && hall >= 0);
  if (forest >= 0 && hall >= 0) {
    CHECK(g.db.graphics.backgrounds[(size_t)forest].whole);
    CHECK(!g.db.graphics.backgrounds[(size_t)hall].whole && g.db.graphics.backgrounds[(size_t)hall].cell.col == 0);
    CHECK_EQ(g.db.graphics.backgrounds[(size_t)g.db.graphics.findBackground("cellar")].cell.col, 1);
  }
  const std::string good = [&] {
    std::ifstream in(ote_test::sourceDir() + "/tests/fixtures/scene-pres/data/graphics.json");
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  }();
  auto errFor = [&](const char* from, const char* to) {
    std::string text = good;
    size_t at = text.find(from);
    if (at != std::string::npos) text.replace(at, std::string(from).size(), to);
    Game::write(g.dir + "/data/graphics.json", text);
    Database db;
    std::string e;
    loadGame(g.dir, db, e);
    return e;
  };
  CHECK(errFor("\"cell\": [1, 0] }", "\"cell\": [5, 0] }").find("background \"cellar\": cell [5, 0] is outside sheet") != std::string::npos);
  CHECK(errFor("\"sheet\": \"bg_forest\" }", "\"sheet\": \"nope\" }").find("background \"forest\": unknown sheet \"nope\"") != std::string::npos);
  CHECK(errFor("\"forest\": {", "\"for est\": {").find("the id must be letters") != std::string::npos);
  CHECK(errFor("\"sheet\": \"bg_forest\" }", "\"sheet\": \"bg_forest\", \"loop\": 1 }").find("unknown key \"loop\"") != std::string::npos);
}
