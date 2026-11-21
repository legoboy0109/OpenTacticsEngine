// Unit tests of the I/O-free pieces of ote-mcp: path syntax, the line diff, argument checking and the PNG helpers.
// (The protocol and every tool are tested end to end by tests/mcp_test.py.)
#include "diff.h"
#include "jpath.h"
#include "png.h"
#include "test.h"
#include "tool.h"

using namespace ote_mcp;

namespace {
Json J(const char* text) {
  Json j;
  std::string e;
  Json::parse(text, j, e);
  return j;
}

std::string resolveLabel(Json& root, const std::string& path, bool create = false, std::string* err = nullptr) {
  std::vector<Seg> segs;
  std::string why;
  if (!parsePath(path, segs, why)) { if (err) *err = why; return "!parse"; }
  ConcretePath cp;
  if (!resolvePathIn(root, segs, cp, why, create)) { if (err) *err = why; return "!resolve"; }
  return pathLabel(root, cp);
}
}  // namespace

TEST(mcp_path_parse) {
  std::vector<Seg> s;
  std::string err;
  CHECK(parsePath("units[archer].stats.hp", s, err));
  CHECK_EQ(s.size(), size_t(4));
  CHECK(s[0].kind == Seg::Key && s[1].kind == Seg::Match && s[1].key == "id" && s[1].matchVal == "archer");
  CHECK(parsePath("[2]", s, err) && s[0].kind == Seg::Index && s[0].index == 2);
  CHECK(parsePath("[-1]", s, err) && s[0].index == -1);
  CHECK(parsePath("events[name=intro].do[0]", s, err) && s[1].key == "name" && s[1].matchVal == "intro");
  CHECK(parsePath("[\"a.b\"].c", s, err) && s[0].kind == Seg::Key && s[0].key == "a.b");
  CHECK(parsePath("", s, err) && s.empty());
  CHECK(!parsePath("a[", s, err));
  CHECK(!parsePath("a[]", s, err));
  CHECK(!parsePath("[\"x", s, err));
  CHECK(!parsePath(std::string(3000, 'a'), s, err));
}

TEST(mcp_path_resolve_and_label) {
  Json doc = J(R"({ "units": [ {"id": "archer", "stats": {"hp": 5}}, {"id": "7", "stats": {}}, {"name": "x"} ], "rules": {} })");
  CHECK_STR(resolveLabel(doc, "units[archer].stats.hp"), "units[archer].stats.hp");
  CHECK_STR(resolveLabel(doc, "units[0]"), "units[archer]");
  CHECK_STR(resolveLabel(doc, "units[-1]"), "units[2]");  // no id on that entry: the index stays
  CHECK_STR(resolveLabel(doc, "units[id=7]"), "units[id=7]");  // an all-digit id must not read as an index
  Json table = J(R"({ "units": [ {"id": "archer", "stats": {}} ] })");
  CHECK_STR(resolveLabel(table, "[archer].stats"), "units[archer].stats");  // the table shorthand (a file with one table key)
  CHECK_STR(resolveLabel(doc, "[archer].stats"), "!resolve");               // not with several keys
  CHECK_STR(resolveLabel(doc, "units[name=x]"), "units[2]");
  std::string err;
  CHECK_STR(resolveLabel(doc, "units[nobody]", false, &err), "!resolve");
  CHECK(err.find("ids there: archer, 7") != std::string::npos);
  CHECK_STR(resolveLabel(doc, "units[9]", false, &err), "!resolve");
  CHECK(err.find("out of range") != std::string::npos);
  CHECK_STR(resolveLabel(doc, "rules.zz", false, &err), "!resolve");
  CHECK(err.find("keys there") != std::string::npos);
  CHECK_STR(resolveLabel(doc, "rules.zz", true), "rules.zz");  // a new last key is allowed when creating
  CHECK_STR(resolveLabel(doc, "rules.a.b", true), "rules.a.b");
  CHECK(doc["rules"]["a"].isObject());  // the missing middle object was made
  CHECK_STR(resolveLabel(doc, "units.x", false, &err), "!resolve");
}

TEST(mcp_value_at) {
  Json doc = J(R"({ "a": [ {"b": 1} ] })");
  std::vector<Seg> s;
  std::string err;
  ConcretePath cp;
  parsePath("a[0].b", s, err);
  CHECK(resolvePathIn(doc, s, cp, err));
  CHECK_EQ(valueAt(doc, cp)->asInt(), 1);
  *valueAt(doc, cp) = Json::makeNumber(2);
  CHECK_STR(doc.dump(0), "{\"a\":[{\"b\":2}]}");
  cp.push_back(Step{false, 0, "nothing"});
  CHECK(valueAt(doc, cp) == nullptr);
}

TEST(mcp_diff) {
  CHECK(unifiedDiff("a\nb\n", "a\nb\n").text.empty());
  const DiffResult d = unifiedDiff("a\nb\nc\nd\ne\nf\ng\nh\n", "a\nb\nc\nX\ne\nf\ng\nh\n", 1);
  CHECK_EQ(d.added, 1);
  CHECK_EQ(d.removed, 1);
  CHECK(d.text.find("-d\n") != std::string::npos && d.text.find("+X\n") != std::string::npos);
  CHECK(d.text.find("-a\n") == std::string::npos);  // context lines are ' ', far lines are absent
  CHECK(d.text.find(" c\n") != std::string::npos && d.text.find(" e\n") != std::string::npos && d.text.find(" a\n") == std::string::npos);
  const DiffResult add = unifiedDiff("", "x\ny\n");
  CHECK_EQ(add.added, 2);
  CHECK_EQ(add.removed, 0);
  std::string big, big2;
  for (int i = 0; i < 1000; ++i) { big += "line" + std::to_string(i) + "\n"; big2 += "other" + std::to_string(i) + "\n"; }
  const DiffResult cut = unifiedDiff(big, big2, 2, 50);
  CHECK(cut.truncated && cut.text.find("diff cut after 50 lines") != std::string::npos);
}

TEST(mcp_argument_schema_check) {
  const Json schema = sObject({{"name", sType("string", "n")}, {"n", sInt("count", 1, 5)}, {"mode", sEnum({"a", "b"}, "m")}, {"list", sArray(sType("string", ""), "l")}}, {"name"});
  CHECK_STR(checkAgainstSchema(schema, J(R"({"name":"x"})")), "");
  CHECK(checkAgainstSchema(schema, J(R"({})")).find("needs the key \"name\"") != std::string::npos);
  CHECK(checkAgainstSchema(schema, J(R"({"name":1})")).find("arguments.name") != std::string::npos);
  CHECK(checkAgainstSchema(schema, J(R"({"name":"x","n":9})")).find("above the maximum 5") != std::string::npos);
  CHECK(checkAgainstSchema(schema, J(R"({"name":"x","n":1.5})")).find("whole number") != std::string::npos);
  CHECK(checkAgainstSchema(schema, J(R"({"name":"x","mode":"c"})")).find("one of: a, b") != std::string::npos);
  CHECK(checkAgainstSchema(schema, J(R"({"name":"x","zz":1})")).find("unknown key \"zz\"") != std::string::npos);
  CHECK(checkAgainstSchema(schema, J(R"({"name":"x","list":["a",2]})")).find("list[1]") != std::string::npos);
  CHECK(checkAgainstSchema(schema, J(R"([])")).find("object") != std::string::npos);
}

TEST(mcp_png_helpers) {
  Image img;
  img.width = 40;
  img.height = 30;
  img.rgb.assign(40 * 30 * 3, 0);
  for (int y = 0; y < 30; ++y)
    for (int x = 0; x < 40; ++x) img.rgb[(size_t)(y * 40 + x) * 3] = (uint8_t)(x * 6);
  const std::string png = encodePng(img);
  CHECK(png.compare(0, 8, "\x89PNG\r\n\x1a\n") == 0);
  CHECK(png.find("IHDR") == 12 && png.find("IDAT") != std::string::npos && png.rfind("IEND") + 8 == png.size());
  CHECK(png.size() < img.rgb.size());  // it compressed (a flat gradient)
  const Image small = shrinkToFit(img, 20);
  CHECK_EQ(small.width, 20);
  CHECK_EQ(small.height, 15);
  CHECK_EQ(shrinkToFit(img, 100).width, 40);  // never enlarged
  // A BMP of 2 x 2 pixels, 24 bit, bottom row first, rows padded to 4 bytes.
  std::string bmp(54, '\0');
  bmp[0] = 'B'; bmp[1] = 'M';
  bmp[10] = 54;
  bmp[14] = 40;
  bmp[18] = 2; bmp[22] = 2; bmp[26] = 1; bmp[28] = 24;
  const unsigned char rows[] = {0, 0, 255, 0, 255, 0, 0, 0,   // bottom: red, green (B G R order)
                                255, 0, 0, 255, 255, 255, 0, 0};  // top: blue, white
  bmp.append(reinterpret_cast<const char*>(rows), sizeof rows);
  Image out;
  std::string err;
  CHECK(decodeBmp(bmp, out, err));
  CHECK_EQ(out.width, 2);
  CHECK(out.rgb[0] == 0 && out.rgb[1] == 0 && out.rgb[2] == 255);  // top-left is blue
  CHECK(out.rgb[6] == 255 && out.rgb[7] == 0 && out.rgb[8] == 0);  // bottom-left is red
  CHECK(!decodeBmp("nonsense", out, err));
}
