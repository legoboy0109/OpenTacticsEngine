// data/campaign.json: loading, defaults, and a refusal with a clear message for every kind of mistake
// (docs/EVENTS.md "Campaign").
#include "campaign_fixture.h"

using namespace camptest;

namespace {

// The loader's message for a campaign.json text ("" = accepted).
std::string refuse(const std::string& campaign) {
  Game g;
  return g.loadError("camp_loader", campaign);
}

bool has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

// A one-chapter campaign with extra pieces: `top` after "chapters" at the top level, `chapter` inside the chapter.
std::string one(const std::string& chapter, const std::string& top = "") {
  return "{ " + top + " \"chapters\": [ { \"id\": \"a\", \"map\": \"ford\"" + (chapter.empty() ? "" : ", " + chapter) + " } ] }";
}

}  // namespace

TEST(campaign_loads_the_fixture_campaign) {
  Game g;
  CHECK(g.load("camp_loader"));
  CHECK(g.db.campaign != nullptr);
  const CampaignDef& c = *g.db.campaign;
  CHECK_STR(c.name, "The Ford War");
  CHECK_EQ((int)c.chapters.size(), 5);
  CHECK(!c.fallenReturn && !c.endOnDefeat);
  CHECK(c.freeBattles == CampaignDef::FreeBattles::List);
  CHECK_STR(c.chapters[0].name, "The Ford");
  CHECK_EQ(c.chapters[1].map, g.db.findMap("mill"));
  CHECK_STR(c.chapters[2].next, "ch4");
  CHECK_EQ((int)c.chapters[0].before.size(), 3);
  CHECK_EQ((int)c.chapters[0].after.size(), 3);
}

TEST(campaign_missing_file_means_no_campaign) {
  Game g;
  CHECK(g.load("camp_loader", ""));
  CHECK(g.db.campaign == nullptr);
  Playthrough pt;
  CHECK(!pt.begin(g.db));
}

TEST(campaign_defaults) {
  Game g;
  CHECK(g.load("camp_loader", one("")));
  const CampaignDef& c = *g.db.campaign;
  CHECK(!c.fallenReturn && !c.endOnDefeat);  // gone, retry
  CHECK(c.freeBattles == CampaignDef::FreeBattles::None);
  CHECK_STR(c.chapters[0].name, "a");  // name defaults to the id
  CHECK_STR(c.name, "Campaign");
  CHECK(g.load("camp_loader", one("", R"("rules": {"fallen":"return","onDefeat":"end"}, "freeBattles":"all",)")));
  CHECK(g.db.campaign->fallenReturn && g.db.campaign->endOnDefeat);
  CHECK(g.db.campaign->freeBattles == CampaignDef::FreeBattles::All);
}

TEST(campaign_loader_refuses_structure_mistakes) {
  std::string e = refuse(R"({ "chapters": [] })");
  CHECK(has(e, "campaign.json") && has(e, "\"chapters\" must not be empty"));
  CHECK(has(refuse("{}"), "missing required \"chapters\""));
  e = refuse(R"({ "chapters": [ {"id":"a","map":"ford"}, {"id":"a","map":"mill"} ] })");
  CHECK(has(e, "chapter \"a\"") && has(e, "duplicate chapter id"));
  e = refuse(R"({ "chapters": [ {"id":"a","map":"nowhere"} ] })");
  CHECK(has(e, "chapter \"a\"") && has(e, "unknown map \"nowhere\""));
  CHECK(has(refuse(R"({ "chapters": [ {"id":"a"} ] })"), "missing required \"map\""));
  CHECK(has(refuse(R"({ "chapters": [ {"id":"a b","map":"ford"} ] })"), "\"id\" must be letters"));
  CHECK(has(refuse(R"({ "chapters": [ "ford" ] })"), "must be an object"));
  e = refuse(one(R"("next": "zzz")"));
  CHECK(has(e, "\"next\" names unknown chapter \"zzz\""));
}

TEST(campaign_loader_refuses_unknown_keys) {
  CHECK(has(refuse(R"({ "chapters": [ {"id":"a","map":"ford"} ], "chaptres": 1 })"), "unknown key \"chaptres\""));
  CHECK(has(refuse(one(R"("mapp": "x")")), "unknown key \"mapp\""));
  CHECK(has(refuse(one("", R"("rules": {"fallen":"gone","lives":3},)")), "unknown key \"lives\""));
  CHECK(has(refuse(one(R"("before": [ {"say":"x","volume":3} ])")), "unknown key \"volume\""));
}

TEST(campaign_loader_refuses_bad_rule_values) {
  CHECK(has(refuse(one("", R"("rules": {"fallen":"maybe"},)")), "\"fallen\" must be \"gone\" or \"return\""));
  CHECK(has(refuse(one("", R"("rules": {"onDefeat":"quit"},)")), "\"onDefeat\" must be \"retry\" or \"end\""));
  CHECK(has(refuse(one("", R"("rules": {"fallen":1},)")), "\"fallen\" must be a string"));
  CHECK(has(refuse(one("", R"("rules": 5,)")), "\"rules\" must be an object"));
  CHECK(has(refuse(one("", R"("freeBattles":"some",)")), "must be \"none\", \"all\" or a list"));
  CHECK(has(refuse(one("", R"("freeBattles":["nowhere"],)")), "unknown map \"nowhere\""));
  CHECK(has(refuse(one("", R"("freeBattles":[3],)")), "must be a map id"));
  CHECK(has(refuse(one("", R"("freeBattles":3,)")), "\"freeBattles\" must be"));
}

TEST(campaign_loader_refuses_goto_mistakes) {
  std::string e = refuse(one(R"("before": [ {"goto":"a"} ])"));
  CHECK(has(e, "\"before\"") && has(e, "command 1 (goto)") && has(e, "only works in a chapter's \"after\""));
  e = refuse(one(R"("before": [ {"if":[{"switch":"bridge_cut"}],"then":[{"goto":"a"}]} ])"));
  CHECK(has(e, "\"then\"") && has(e, "only works in a chapter's \"after\""));
  e = refuse(one(R"("after": [ {"goto":"zzz"} ])"));
  CHECK(has(e, "\"goto\" names unknown chapter \"zzz\""));
  e = refuse(one(R"("after": [ {"if":[{"dead":"x"}],"then":[],"else":[{"goto":"zzz"}]} ])"));
  CHECK(has(e, "\"goto\" names unknown chapter \"zzz\""));
  CHECK(has(refuse(one(R"("after": [ {"goto":5} ])")), "\"goto\" must be a chapter id"));
  // A goto to a later chapter and to itself is fine.
  Game g;
  CHECK(g.load("camp_loader", R"({ "chapters": [ {"id":"a","map":"ford","after":[{"goto":"b"}]}, {"id":"b","map":"mill","after":[{"goto":"b"}]} ] })"));
}

TEST(campaign_loader_refuses_bad_scene_commands) {
  std::string e = refuse(one(R"("before": [ {"setSwitch":"nope","to":true} ])"));
  CHECK(has(e, "chapter \"a\"") && has(e, "\"before\": command 1 (setSwitch)") && has(e, "unknown switch \"nope\""));
  CHECK(has(refuse(one(R"("before": [ {"setVariable":"nope","to":1} ])")), "unknown variable \"nope\""));
  CHECK(has(refuse(one(R"("before": [ {"addVariable":"kills"} ])")), "missing required \"by\""));
  CHECK(has(refuse(one(R"("before": [ {"giveResource":{"silver":5},"side":"blue"} ])")), "unknown resource \"silver\""));
  CHECK(has(refuse(one(R"("before": [ {"giveResource":{"gold":5}} ])")), "missing required \"side\""));
  CHECK(has(refuse(one(R"("before": [ {"giveResource":{"gold":5},"side":"b l"} ])")), "\"side\" must be a side id"));
  CHECK(has(refuse(one(R"("before": [ {"say":"x","speaker":"a b"} ])")), "\"speaker\" must be a roster unit id"));
  CHECK(has(refuse(one(R"("before": [ {"say":"x","speaker":"a","speakerName":"b"} ])")), "not both"));
  CHECK(has(refuse(one(R"("before": [ {"say":5} ])")), "\"say\" must be the text"));
  CHECK(has(refuse(one(R"("before": [ {"addUnit":{}} ])")), "is not a scene command"));
  CHECK(has(refuse(one(R"("before": [ {"say":"x","setSwitch":"met_wanderer","to":true} ])")), "has two commands"));
  CHECK(has(refuse(one(R"("before": [ {"if":[{"alive":"a b"}],"then":[]} ])")), "\"alive\" must be a roster unit id"));
  CHECK(has(refuse(one(R"("before": [ {"if":[{"round":{}}],"then":[]} ])")), "is not a condition"));
  CHECK(has(refuse(one(R"("before": [ {"if":[{"variable":"kills","op":"~","value":1}],"then":[]} ])")), "\"op\" must be one of"));
  CHECK(has(refuse(one(R"("before": [ {"if":[]} ])")), "missing required \"then\""));
  CHECK(has(refuse(one(R"("before": 5)")), "must be a list of commands"));
}

TEST(campaign_unreachable_chapter_is_not_an_error) {
  Game g;
  CHECK(g.load("camp_loader", R"({ "chapters": [ {"id":"a","map":"ford","next":"c"}, {"id":"b","map":"mill"}, {"id":"c","map":"keep"} ] })"));
  CHECK_EQ((int)g.db.campaign->chapters.size(), 3);
}

TEST(campaign_damaged_json_is_reported) {
  CHECK(has(refuse("{ \"chapters\": [ "), "campaign.json"));
  CHECK(has(refuse("[1,2]"), "campaign.json"));
}

TEST(campaign_free_battles_query) {
  Game g;
  CHECK(g.load("camp_loader"));  // the list ["skirmish"]
  CHECK(freeBattleAllowed(g.db, "skirmish"));
  CHECK(!freeBattleAllowed(g.db, "ford"));
  CHECK(!freeBattleAllowed(g.db, "nowhere"));
  CHECK(g.load("camp_loader", one("", R"("freeBattles":"none",)")));
  CHECK(!freeBattleAllowed(g.db, "ford") && !freeBattleAllowed(g.db, "skirmish"));
  CHECK(g.load("camp_loader", one("", R"("freeBattles":"all",)")));
  CHECK(freeBattleAllowed(g.db, "ford") && freeBattleAllowed(g.db, "mill") && !freeBattleAllowed(g.db, "nowhere"));
  CHECK(g.load("camp_loader", ""));  // no campaign: every map is a single battle, as before
  CHECK(freeBattleAllowed(g.db, "ford") && freeBattleAllowed(g.db, "keep"));
}
