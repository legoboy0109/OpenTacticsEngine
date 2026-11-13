#include <cstdio>

#include "core/base/json.h"
#include "test.h"

using namespace ote;

static bool parseOk(const std::string& text, Json& out) {
  std::string err;
  bool ok = Json::parse(text, out, err);
  if (!ok) std::printf("  parse error: %s\n", err.c_str());
  return ok;
}

static std::string parseErr(const std::string& text) {
  Json j;
  std::string err;
  if (Json::parse(text, j, err)) return "<no error>";
  return err;
}

TEST(json_scalars_and_types) {
  Json j;
  CHECK(parseOk("[null, true, false, 12, -3.5, 1e2, \"hi\"]", j));
  CHECK(j.isArray());
  CHECK_EQ(j.size(), size_t(7));
  CHECK(j[size_t(0)].isNull());
  CHECK(j[size_t(1)].asBool());
  CHECK(j[size_t(2)].isBool() && !j[size_t(2)].asBool(true));
  CHECK_EQ(j[size_t(3)].asInt(), 12);
  CHECK(j[size_t(4)].asNumber() == -3.5);
  CHECK_EQ(j[size_t(4)].asInt(), -3);  // truncates toward zero
  CHECK_EQ(j[size_t(5)].asInt(), 100);
  CHECK_STR(j[size_t(6)].asString(), "hi");
  // Wrong-type reads return the default.
  CHECK_EQ(j[size_t(6)].asInt(7), 7);
  CHECK_STR(j[size_t(3)].asString("d"), "d");
  CHECK(j[size_t(1)].asNumber(9) == 9);
}

TEST(json_nesting_and_missing_keys) {
  Json j;
  CHECK(parseOk("{\"a\": {\"b\": [1, {\"c\": \"deep\"}]}, \"z\": 1}", j));
  CHECK_STR(j["a"]["b"][size_t(1)]["c"].asString(), "deep");
  CHECK(j["nope"].isNull());
  CHECK(j["nope"]["x"]["y"][size_t(3)].isNull());
  CHECK_EQ(j["nope"]["x"].asInt(5), 5);
  CHECK(j["a"]["b"][size_t(9)].isNull());
  CHECK(j.has("z"));
  CHECK(!j.has("q"));
  CHECK_EQ(j["a"].size(), size_t(1));
  CHECK_EQ(j["z"].size(), size_t(0));
}

TEST(json_object_keeps_order) {
  Json j;
  CHECK(parseOk("{\"b\": 1, \"a\": 2, \"c\": 3}", j));
  CHECK_EQ(j.members().size(), size_t(3));
  CHECK_STR(j.members()[0].first, "b");
  CHECK_STR(j.members()[1].first, "a");
  CHECK_STR(j.members()[2].first, "c");
}

TEST(json_comments_and_trailing_commas) {
  Json j;
  CHECK(parseOk("// head\n{ /* block\n comment */ \"a\": [1, 2, ], // tail\n \"b\": {\"x\": 1,},\n}\n// end", j));
  CHECK_EQ(j["a"].size(), size_t(2));
  CHECK_EQ(j["b"]["x"].asInt(), 1);
  CHECK(parseOk("[]", j));
  CHECK(parseOk("{}", j));
}

TEST(json_string_escapes_and_unicode) {
  Json j;
  CHECK(parseOk("\"a\\n\\t\\\"\\\\\\/\\b\\f\\r\"", j));
  CHECK_STR(j.asString(), "a\n\t\"\\/\b\f\r");
  CHECK(parseOk("\"\\u0041\\u00e9\\u20ac\"", j));
  CHECK_STR(j.asString(), "A\xC3\xA9\xE2\x82\xAC");
  // Surrogate pair: U+1F600
  CHECK(parseOk("\"\\ud83d\\ude00\"", j));
  CHECK_STR(j.asString(), "\xF0\x9F\x98\x80");
  CHECK(!parseErr("\"\\ud83d\"").empty() && parseErr("\"\\ud83d\"") != "<no error>");
  CHECK(parseErr("\"\\q\"") != "<no error>");
  CHECK(parseErr("\"\\u12\"") != "<no error>");
}

TEST(json_error_line_numbers) {
  CHECK_STR(parseErr("{\n  \"a\": 1,\n  \"b\": ?\n}"), "line 3: unexpected character '?'");
  CHECK(parseErr("{\"a\": 1\n\n").rfind("line 3:", 0) == 0);
  CHECK(parseErr("[1, 2\n 3]").rfind("line 2:", 0) == 0);
  CHECK(parseErr("{\"a\" 1}").rfind("line 1:", 0) == 0);
  CHECK(parseErr("{} garbage").rfind("line 1:", 0) == 0);
  CHECK(parseErr("{} {}") != "<no error>");
  CHECK(parseErr("/* open\n\n") .rfind("line 1:", 0) == 0);
  CHECK(parseErr("") != "<no error>");
  CHECK(parseErr("\"abc") != "<no error>");
  CHECK(parseErr("tru") != "<no error>");
}

TEST(json_rejects_odd_numbers) {
  CHECK(parseErr("0x10") != "<no error>");
  CHECK(parseErr("[inf]") != "<no error>");
  CHECK(parseErr("[nan]") != "<no error>");
  CHECK(parseErr("[-]") != "<no error>");
}

TEST(json_depth_limit) {
  std::string deep(300, '[');
  deep += std::string(300, ']');
  CHECK(parseErr(deep).find("too deep") != std::string::npos);
  std::string fine(100, '[');
  fine += std::string(100, ']');
  Json j;
  CHECK(parseOk(fine, j));
}

TEST(json_builders_and_set_replaces) {
  Json o;
  o.set("n", Json::makeNumber(1));
  o.set("s", Json::makeString("x"));
  o.set("n", Json::makeNumber(2));
  CHECK(o.isObject());
  CHECK_EQ(o.size(), size_t(2));
  CHECK_EQ(o["n"].asInt(), 2);
  Json a;
  a.push(Json::makeBool(true));
  a.push(Json::makeNumber(3));
  CHECK(a.isArray());
  CHECK_EQ(a.size(), size_t(2));
}

TEST(json_dump_numbers_and_escapes) {
  Json a = Json::makeArray();
  a.push(Json::makeNumber(3));
  a.push(Json::makeNumber(-2));
  a.push(Json::makeNumber(0.5));
  a.push(Json::makeString("q\"\\\n\x01"));
  CHECK_STR(a.dump(0), "[3,-2,0.5,\"q\\\"\\\\\\n\\u0001\"]");
  Json o = Json::makeObject();
  o.set("k", Json::makeNumber(1));
  CHECK_STR(o.dump(2), "{\n  \"k\": 1\n}");
  CHECK_STR(Json::makeArray().dump(2), "[]");
}

TEST(json_dump_round_trip) {
  const char* src =
      "{\"name\": \"caf\\u00e9 \\ud83d\\ude00\", \"n\": [1, 2.25, -7, 1e-7, 123456789012],"
      " \"o\": {\"t\": true, \"f\": false, \"z\": null, \"e\": {}, \"a\": []}, \"s\": \"l1\\nl2\\t\\\"\"}";
  Json a, b, c;
  CHECK(parseOk(src, a));
  CHECK(parseOk(a.dump(2), b));
  CHECK(parseOk(a.dump(0), c));
  CHECK_STR(a.dump(0), b.dump(0));
  CHECK_STR(a.dump(0), c.dump(0));
  CHECK(b["n"][size_t(1)].asNumber() == 2.25);
  CHECK(b["n"][size_t(3)].asNumber() == 1e-7);
  CHECK(b["n"][size_t(4)].asNumber() == 123456789012.0);
  CHECK_STR(b["s"].asString(), "l1\nl2\t\"");
}

TEST(json_file_helpers) {
  std::string path = ote_test::sourceDir() + "/tests/fixtures/base/game.json";
  Json j;
  std::string err;
  CHECK(loadJsonFile(path, j, err));
  CHECK_EQ(j["formatVersion"].asInt(), 1);

  std::string missing = ote_test::sourceDir() + "/tests/fixtures/does_not_exist.json";
  CHECK(!loadJsonFile(missing, j, err));
  CHECK(err.find("does_not_exist.json") != std::string::npos);

  std::string tmp = "ote_json_test.tmp";
  const std::string body = "{\"a\": [1,\n 2,\n x]}";
  CHECK(writeTextFile(tmp, body));
  CHECK(!loadJsonFile(tmp, j, err));
  CHECK(err.find(tmp) == 0);
  CHECK(err.find("line 3") != std::string::npos);
  std::string back;
  CHECK(readTextFile(tmp, back));
  CHECK_STR(back, body);
  std::remove(tmp.c_str());
}
