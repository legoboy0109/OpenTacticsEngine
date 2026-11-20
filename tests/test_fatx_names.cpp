// The original Xbox's file name rules (core/base/fatx_names.h, docs/XBOX_OG.md).
#include "core/base/fatx_names.h"
#include "test.h"

using namespace ote;

TEST(fatx_accepts_the_demo_style_names) {
  CHECK_STR(fatxNameProblem("demo-economy"), std::string());
  CHECK_STR(fatxNameProblem("continue.json.tmp"), std::string());
  CHECK_STR(fatxNameProblem("Hero_Sprites (2) [x]@1.png"), std::string());
  CHECK_STR(fatxNameProblem(std::string(42, 'a')), std::string());
}

TEST(fatx_rejects_long_names_bad_characters_and_non_names) {
  CHECK(!fatxNameProblem(std::string(43, 'a')).empty());
  CHECK(!fatxNameProblem("a+b").empty());
  CHECK(!fatxNameProblem("a,b").empty());
  CHECK(!fatxNameProblem("a=b").empty());
  CHECK(!fatxNameProblem("a;b").empty());
  CHECK(!fatxNameProblem("q?").empty());
  CHECK(!fatxNameProblem("caf\xC3\xA9").empty());  // non-ASCII (UTF-8 bytes)
  CHECK(!fatxNameProblem("").empty());
  CHECK(!fatxNameProblem("..").empty());
  CHECK(fatxNameProblem("a+b").find("'+'") != std::string::npos);
}

TEST(fatx_path_problems_name_the_component) {
  CHECK(fatxPathProblems("games/demo-srpg/data/maps/bridge.json").empty());
  std::vector<std::string> p = fatxPathProblems("games\\mine\\sprites/hero+.png");
  CHECK_EQ(p.size(), (size_t)1);
  CHECK(p[0].find("hero+.png") != std::string::npos);
  CHECK(!fatxPathProblems(std::string(300, 'a')).empty());  // one name too long and a path too long
}

TEST(fatx_case_clashes) {
  std::vector<std::string> n = {"Hero.png", "hero.png", "other.png"};
  CHECK_EQ(fatxCaseClashes(n).size(), (size_t)1);
  CHECK(fatxCaseClashes({"a.png", "b.png"}).empty());
}

TEST(fatx_safe_name_keeps_good_names_and_encodes_the_rest) {
  CHECK_STR(fatxSafeName("demo-srpg"), std::string("demo-srpg"));
  CHECK_STR(fatxSafeName("a+b"), std::string("a~2Bb"));
  CHECK_STR(fatxSafeName("a/b"), std::string("a~2Fb"));
  CHECK_STR(fatxSafeName("x~y+"),std::string("x~7Ey~2B"));  // a name that needs encoding encodes '~' too
  CHECK_STR(fatxSafeName(""), std::string("~00"));
  std::string longName(60, 'a');
  std::string s = fatxSafeName(longName);
  CHECK_EQ(s.size(), kFatxMaxName);
  CHECK(fatxNameProblem(s).empty());
  CHECK(fatxSafeName(std::string(60, 'a') + "1") != s);  // different long names stay different
  CHECK(fatxNameProblem(fatxSafeName("caf\xC3\xA9 +,;=?*\"<>|:\\/")).empty());
}
