// The args file of the UWP and original Xbox builds (platform/common/launch_args.h).
#include "platform/common/launch_args.h"
#include "test.h"

using namespace ote;

TEST(launch_args_splits_words_and_environment) {
  std::vector<std::pair<std::string, std::string>> env;
  std::vector<std::string> a = parseLaunchArgs("--game demo-srpg\r\nOTE_LOG_LEVEL=debug --input \"up,wait:3\"\n--frames 5", env);
  CHECK_EQ(a.size(), (size_t)6);
  CHECK_STR(a[0], "--game");
  CHECK_STR(a[1], "demo-srpg");
  CHECK_STR(a[3], "up,wait:3");
  CHECK_EQ(env.size(), (size_t)1);
  CHECK_STR(env[0].first, "OTE_LOG_LEVEL");
  CHECK_STR(env[0].second, "debug");
}

TEST(launch_args_comments_quotes_and_odd_equals) {
  std::vector<std::pair<std::string, std::string>> env;
  std::vector<std::string> a = parseLaunchArgs("# a comment line\n--font \"my font.ttf\"\n--x=1 A-B=2 =3\n", env);
  CHECK_STR(a[0], "--font");
  CHECK_STR(a[1], "my font.ttf");
  CHECK_STR(a[2], "--x=1");  // an option, not an environment word
  CHECK_STR(a[3], "A-B=2");  // not a legal name
  CHECK_STR(a[4], "=3");
  CHECK(env.empty());
  CHECK(parseLaunchArgs("", env).empty());
}
