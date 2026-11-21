// Unit tests of the schema engine (editor/core/schema_engine.*, schema_values.cpp, schema_ids.cpp), written without Qt. Most
// cases are the native editor's schema tests restated on the Qt-free API (the native tests keep running on the Qt wrapper, so a
// difference between the two would show in one of them).
#include <string>
#include <vector>

#include "schema_engine.h"
#include "schema_ids.h"
#include "test.h"

using namespace ote_editor;
using ote::Json;

namespace {
Json J(const char* text) {
  Json j;
  std::string e;
  Json::parse(text, j, e);
  return j;
}

std::string errorsOf(const SchemaSet& s) {
  std::string all;
  for (const std::string& e : s.errors()) all += e + "\n";
  return all;
}

bool contains(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }
}  // namespace

TEST(schema_engine_parses_every_node_kind) {
  SchemaSet set;
  set.addFromText("t.json", R"({
    "title": "T", "files": ["data/t.json"],
    "rows": { "type": "object", "fields": {
      "a": { "type": "string", "required": true, "help": "h" },
      "b": { "type": "int", "min": 1, "max": 5, "default": 2 },
      "c": { "type": "enum", "values": ["x", "y"] },
      "d": { "type": "ref", "ref": ["data/a.json", "game.json#b"], "values": ["choose"] },
      "e": { "type": "list", "length": 2, "of": { "type": "number" } },
      "f": { "type": "map", "ref": "game.json#stats", "expand": true, "of": { "type": "int" } },
      "g": { "block": true, "alt": "number", "fields": { "h": { "type": "bool" } } },
      "i": { "type": "json" } } } })");
  CHECK_STR(errorsOf(set), "");
  const FileSchema* fs = set.forFile("data/t.json");
  CHECK(fs && fs->rows);
  CHECK_STR(fs->title, "T");
  const SchemaNode& rows = *fs->rows;
  CHECK_EQ(rows.fields.size(), size_t(8));
  CHECK(rows.field("a")->required);
  CHECK_STR(rows.field("a")->help, "h");
  CHECK_EQ(*rows.field("b")->min, 1.0);
  CHECK_EQ(rows.field("b")->deflt.asInt(), 2);
  CHECK_EQ(rows.field("c")->values.size(), size_t(2));
  CHECK_EQ(rows.field("d")->refs.size(), size_t(2));
  CHECK_STR(rows.field("d")->values.front(), "choose");
  CHECK_EQ(rows.field("e")->length, 2);
  CHECK(rows.field("f")->expand && rows.field("f")->kind == SchemaNode::Kind::Map);
  CHECK(rows.field("g")->kind == SchemaNode::Kind::Object && rows.field("g")->block);
  CHECK_STR(rows.field("g")->alt, "number");
  CHECK(rows.field("i")->kind == SchemaNode::Kind::Json);
  CHECK(!set.forFile("data/other.json"));
}

TEST(schema_engine_reports_mistakes_without_stopping) {
  SchemaSet set;
  set.addFromText("broken.json", "{ not json");
  set.addFromText("nofiles.json", R"({ "rows": { "type": "object" } })");
  set.addFromText("badtype.json", R"({ "files": ["x.json"], "rows": { "fields": { "a": { "type": "banana" }, "b": { "type": "enum" },
                                         "c": { "type": "ref" }, "d": { "type": "list" } } } })");
  const std::string all = errorsOf(set);
  CHECK(contains(all, "broken.json: not valid JSON"));
  CHECK(contains(all, "nofiles.json: \"files\" is missing"));
  CHECK(contains(all, "unknown type \"banana\""));
  CHECK(contains(all, "enum without \"values\""));
  CHECK(contains(all, "ref without \"ref\""));
  CHECK(contains(all, "list without \"of\""));
}

TEST(schema_engine_definitions_are_shared_and_recursion_goes_lazy) {
  SchemaSet set;
  set.addFromText("_defs.json", R"({ "defs": {
    "pair": { "type": "list", "length": 2, "of": { "type": "int" } },
    "cond": { "type": "variant", "variants": [
      { "name": "plain", "kind": "string", "node": { "type": "string" } },
      { "name": "all", "key": "all", "node": { "type": "object", "fields": { "all": { "type": "list", "of": { "use": "cond" } } } } } ] } } })");
  set.addFromText("t.json", R"({ "files": ["data/t.json"], "rows": { "type": "object", "fields": {
    "range": { "use": "_defs#pair", "help": "mine" }, "test": { "use": "_defs#cond" } } } })");
  CHECK_STR(errorsOf(set), "");
  const SchemaNode& r = *set.forFile("data/t.json")->rows;
  CHECK_EQ(r.field("range")->length, 2);
  CHECK_STR(r.field("range")->help, "mine");  // the use's own keys win
  const SchemaNode* c = r.field("test");
  CHECK_EQ((int)c->variants.size(), 2);
  const SchemaNode* inner = c->variants[1].node->field("all")->of.get();
  CHECK_EQ((int)inner->variants.size(), 2);
  const SchemaNode* deeper = inner->variants[1].node->field("all")->of.get();
  CHECK(deeper->lazy != nullptr);  // the third level is a placeholder that expands when somebody looks
  CHECK_EQ((int)deeper->resolved().variants.size(), 2);
  const SchemaNode* deepest = deeper->resolved().variants[1].node->field("all")->of.get();
  CHECK(deepest->lazy != nullptr);
  CHECK_EQ((int)deepest->resolved().variants.size(), 2);
  SchemaSet bad;
  bad.addFromText("x.json", R"({ "files": ["a.json"], "root": { "fields": { "k": { "use": "nothere" } } } })");
  CHECK(contains(errorsOf(bad), "unknown def"));
}

TEST(schema_engine_variants_match_by_key_with_unless_kind_and_value) {
  SchemaSet set;
  set.addFromText("t.json", R"({ "files": ["data/t.json"], "root": { "fields": { "cmd": { "type": "variant", "variants": [
    { "name": "focus", "key": "focus", "node": { "type": "object", "fields": { "focus": { "type": "string" } } } },
    { "name": "set number", "key": "set", "unless": ["level"], "summary": "{set} = {to}", "node": { "type": "object", "fields": { "set": { "type": "string" } } } },
    { "name": "set level", "key": "set", "with": ["level"], "summary": "[{who|name}: ]{set} <- {level.of}", "node": { "type": "object", "fields": { "set": { "type": "string" } } } },
    { "name": "wait", "key": "wait", "unless": ["focus"], "node": { "type": "object", "fields": { "wait": { "type": "int" } } } },
    { "name": "word", "value": "sides" },
    { "name": "text", "kind": "string", "node": { "type": "string" } } ] } } } })");
  CHECK_STR(errorsOf(set), "");
  const SchemaNode& v = *set.forFile("data/t.json")->root->field("cmd");
  auto name = [&](const char* json) {
    const int i = variantIndex(v, J(json));
    return i < 0 ? std::string("(none)") : v.variants[(size_t)i].name;
  };
  CHECK_STR(name(R"({"focus":"a"})"), "focus");
  CHECK_STR(name(R"({"set":"a","to":1})"), "set number");
  CHECK_STR(name(R"({"set":"a","level":{"of":"x"}})"), "set level");
  CHECK_STR(name(R"({"wait":3})"), "wait");
  CHECK_STR(name(R"({"focus":"a","wait":3})"), "focus");  // wait is an option of focus here: `unless` keeps it out of "wait"
  CHECK_STR(name(R"("sides")"), "word");                  // a constant beats the kind
  CHECK_STR(name(R"("other")"), "text");
  CHECK_STR(name(R"({"nothing":1})"), "(none)");
  CHECK_STR(schemaSummary("{set} = {to}", J(R"({"set":"a","to":3})")), "a = 3");
  CHECK_STR(schemaSummary("[{who|name}: ]{set} <- {level.of}", J(R"({"name":"Eirik","set":"a","level":{"of":"hero"}})")), "Eirik: a <- hero");
  CHECK_STR(schemaSummary("[{who|name}: ]{set}", J(R"({"set":"a"})")), "a");
  CHECK_STR(schemaSummary("{u}", J(R"({"u":{"unit":"gorm"}})")), "gorm");
  CHECK_STR(schemaSummary("{u}", J(R"({"u":{}})")), "any unit");
  CHECK_STR(schemaSummary("{u}", J(R"({"u":{"side":"red"}})")), "side red");
  CHECK_STR(schemaSummary("{u}", J(R"({"u":null})")), "none");
  CHECK(schemaSummary("{u}", J(("{\"u\":\"" + std::string(300, 'x') + "\"}").c_str())).size() <= 62);  // cut with an ellipsis (3 bytes)
  // Cutting counts characters, not bytes: 100 two-byte letters are cut to 60 characters, never in the middle of one.
  std::string accents;
  for (int i = 0; i < 100; ++i) accents += "\xC3\xA9";
  const std::string cut = schemaSummary("{u}", J(("{\"u\":\"" + accents + "\"}").c_str()));
  CHECK_EQ(cut.size(), size_t(59 * 2 + 3));
}

TEST(schema_engine_when_needs_enable_and_values) {
  SchemaSet set;
  set.addFromText("t.json", R"({ "files": ["data/t.json"], "root": { "fields": {
    "kind": { "type": "enum", "values": ["a", "b"], "default": "a" },
    "x": { "type": "int", "when": { "kind": ["b"] } },
    "blk": { "type": "object", "block": true, "needs": { "ref": "data/ids.json", "hint": "add an id" }, "enable": { "n": 1 }, "fields": { "n": { "type": "int" } } },
    "free": { "type": "map", "free": true, "of": { "type": "int" } } } } })");
  CHECK_STR(errorsOf(set), "");
  const SchemaNode& r = *set.forFile("data/t.json")->root;
  const SchemaNode& x = *r.field("x");
  const Json empty = J("{}"), b = J(R"({"kind":"b"})");
  CHECK(!fieldApplies(r, x, nullptr));
  CHECK(!fieldApplies(r, x, &empty));  // the sibling's default is "a"
  CHECK(fieldApplies(r, x, &b));
  CHECK_STR(schemaDefault(*r.field("blk"), nullptr).dump(0), "{\"n\":1}");  // Enable writes the schema's own value
  CHECK(r.field("free")->free);
  CHECK_STR(r.field("blk")->needs->hint, "add an id");
  SchemaSet bad;
  bad.addFromText("x.json", R"({ "files": ["a.json"], "root": { "fields": { "v": { "type": "variant", "variants": [ { "name": "n", "key": "a", "kind": "string", "node": {} }, { "node": {} } ] } } } })");
  CHECK(contains(errorsOf(bad), "exactly one of"));
  CHECK(contains(errorsOf(bad), "needs a \"name\""));
}

TEST(schema_engine_file_patterns) {
  SchemaSet set;
  set.addFromText("maps.json", R"({ "files": ["maps/*.json"], "root": { "type": "object" } })");
  CHECK(set.forFile("maps/duel.json") != nullptr);
  CHECK(set.forFile("maps/sub/deep.json") == nullptr);  // * does not cross a folder
  CHECK(set.forFile("game.json") == nullptr);
  CHECK(set.forFile("data/maps.json") == nullptr);
}

TEST(schema_engine_value_checks) {
  SchemaSet set;
  set.addFromText("t.json", R"({ "files": ["t.json"], "rows": { "fields": {
      "n": { "type": "int", "min": 0, "max": 10 }, "s": { "type": "string" }, "e": { "type": "enum", "values": ["a"] },
      "p": { "type": "list", "length": 2, "of": { "type": "int" } }, "a": { "type": "object", "alt": "number", "fields": {} } } } })");
  const SchemaNode& r = *set.forFile("t.json")->rows;
  const SchemaNode& n = *r.field("n");
  CHECK(typeOk(n, Json::makeNumber(5)));
  CHECK(!typeOk(n, Json::makeNumber(5.5)));
  CHECK(!typeOk(n, Json::makeString("5")));
  CHECK(typeOk(n, Json::makeNumber(99)));  // right kind, wrong range
  std::string why;
  CHECK(!schemaAccepts(n, Json::makeNumber(99), &why));
  CHECK(contains(why, "above the maximum 10"));
  CHECK(!schemaAccepts(n, Json::makeNumber(-1), &why));
  CHECK(contains(why, "below the minimum 0"));
  CHECK(schemaAccepts(n, Json::makeNumber(0)) && schemaAccepts(n, Json::makeNumber(10)));
  CHECK(!schemaAccepts(n, Json::makeNumber(2.5), &why));
  CHECK(contains(why, "whole number"));
  CHECK(typeOk(*r.field("e"), Json::makeString("zzz")));
  CHECK(!schemaAccepts(*r.field("e"), Json::makeString("zzz"), &why));
  CHECK(schemaAccepts(*r.field("e"), Json::makeString("a")));
  CHECK(!typeOk(*r.field("p"), J("[1,2,3]")));
  CHECK(typeOk(*r.field("p"), J("[1,2]")));
  const SchemaNode& alt = *r.field("a");
  CHECK(typeOk(alt, Json::makeNumber(1)) && isAlt(alt, Json::makeNumber(1)));
  CHECK(!isAlt(alt, J("{}")));
  CHECK(!typeOk(alt, Json::makeString("x")));
  CHECK(!schemaAccepts(*r.field("s"), Json::makeNumber(1), &why));
  CHECK(contains(why, "text"));
}

TEST(schema_engine_defaults_and_samples) {
  SchemaSet set;
  set.addFromText("t.json", R"({ "files": ["t.json"], "rows": { "fields": {
      "id": { "type": "string", "required": true }, "n": { "type": "int", "min": 3 }, "b": { "type": "bool", "default": true },
      "r": { "type": "ref", "ref": "ids" }, "m": { "type": "map", "ref": "ids", "of": { "type": "int" } },
      "blk": { "block": true, "fields": { "first": { "type": "int", "min": 2 }, "second": { "type": "string" } } },
      "lst": { "type": "list", "of": { "type": "int" } }, "pair": { "type": "list", "length": 2, "of": { "type": "int" } },
      "own": { "type": "int", "sample": 42 },
      "when": { "type": "object", "fields": { "kind": { "type": "enum", "values": ["a", "b"], "required": true },
                                              "needsB": { "type": "int", "required": true, "when": { "kind": ["b"] } } } } } } })");
  const SchemaNode& r = *set.forFile("t.json")->rows;
  const IdLookup ids = [](const std::vector<std::string>&) { return std::vector<std::string>{"one", "two"}; };
  const Json d = schemaDefault(r, ids);
  CHECK(d.has("id") && !d.has("n"));  // a new entry: required keys only
  CHECK_EQ(schemaDefault(*r.field("b"), ids).asBool(), true);
  CHECK_STR(schemaDefault(*r.field("r"), ids).asString(), "one");
  CHECK_EQ(schemaSample(*r.field("n"), ids).asInt(), 3);
  CHECK_EQ(schemaSample(*r.field("b"), ids).asBool(), false);
  const Json blk = schemaSample(*r.field("blk"), ids);
  CHECK_EQ(blk["first"].asInt(), 2);
  CHECK_STR(schemaSample(*r.field("m"), ids).dump(0), "{\"one\":1}");
  CHECK_EQ(schemaSample(*r.field("lst"), ids).size(), size_t(1));
  CHECK_EQ(schemaSample(*r.field("pair"), ids).size(), size_t(2));
  CHECK_EQ(schemaSample(*r.field("own"), ids).asInt(), 42);
  // A required key that applies only to another kind is left out while the kind says otherwise.
  const Json w = schemaDefault(*r.field("when"), ids);
  CHECK(w.has("kind") && !w.has("needsB"));
  // No id lookup at all: references start empty instead of crashing.
  CHECK_STR(schemaDefault(*r.field("r"), nullptr).asString(), "");
}

TEST(schema_engine_variant_defaults_use_the_sample) {
  SchemaSet set;
  set.addFromText("t.json", R"({ "files": ["t.json"], "root": { "fields": { "v": { "type": "variant", "variants": [
      { "name": "word", "value": "sides" },
      { "name": "obj", "key": "hp", "sample": { "hp": 7 }, "node": { "type": "object", "fields": { "hp": { "type": "int", "required": true } } } },
      { "name": "plain", "key": "x", "node": { "type": "object", "fields": { "x": { "type": "int", "required": true } } } } ] } } } })");
  const SchemaNode& v = *set.forFile("t.json")->root->field("v");
  CHECK_STR(schemaDefault(v, nullptr).dump(0), "\"sides\"");
  CHECK_STR(schemaVariantDefault(v.variants[1], nullptr).dump(0), "{\"hp\":7}");
  CHECK_STR(schemaVariantDefault(v.variants[2], nullptr).dump(0), "{\"x\":0}");
  CHECK_STR(schemaVariantSample(v.variants[2], nullptr).dump(0), "{\"x\":1}");
}

TEST(schema_ids_sources_resolve_from_documents) {
  const Json game = J(R"({ "stats": [ {"id": "hp"}, {"id": "str"} ], "damageTypes": [ {"id": "fire"} ], "words": ["a", "b"],
                           "rules": { "ai": { "weights": { "bold": {}, "careful": {} } } } })");
  const Json units = J(R"({ "units": [ {"id": "lancer", "tags": ["mounted", "pierce"], "group": "L"}, {"id": "archer", "tags": "ranged", "group": "A"} ] })");
  const Json empty = J(R"({ "things": [] })");
  const DocLookup docs = [&](const std::string& rel) -> const Json* {
    if (rel == "game.json") return &game;
    if (rel == "data/units.json") return &units;
    if (rel == "data/empty.json") return &empty;
    return nullptr;
  };
  const Json none;
  auto ids = [&](std::vector<std::string> src) { return resolveIdSources(none, docs, src); };
  using V = std::vector<std::string>;
  CHECK(ids({"data/units.json"}) == (V{"lancer", "archer"}));
  CHECK(ids({"game.json#stats", "game.json#damageTypes"}) == (V{"hp", "str", "fire"}));
  CHECK(ids({"game.json#words"}) == (V{"a", "b"}));  // plain strings count
  CHECK(ids({"data/empty.json"}).empty());           // an empty table: the single key's list, which is empty
  CHECK(ids({"data/missing.json"}).empty());
  CHECK(ids({"game.json#nothing.here"}).empty());
  CHECK(ids({"tags:data/units.json"}) == (V{"mounted", "pierce", "ranged"}));
  CHECK(ids({"field:group:data/units.json"}) == (V{"L", "A"}));
  CHECK(ids({"keys:game.json#rules.ai.weights"}) == (V{"bold", "careful"}));
  CHECK(ids({"data/units.json", "data/units.json"}) == (V{"lancer", "archer"}));  // no duplicates
  CHECK_STR(sourceFile("keys:game.json#a.b"), "game.json");
  CHECK_STR(sourceFile("field:group:data/units.json"), "data/units.json");
  CHECK_STR(sourceFile("@sides"), "");
}

TEST(schema_ids_sources_of_the_edited_file) {
  const Json map = J(R"({ "sides": [ {"id": "blue", "team": "allies"}, {"id": "red"}, {"id": "gold", "team": "allies"} ],
                          "units": [ {"id": "a1"}, {"character": "eirik"} ],
                          "changes": { "bridge": {}, "gate": {} },
                          "events": [ { "do": [ { "addUnit": { "id": "late", "character": "kell" } },
                                                { "if": {}, "then": [ { "addUnit": { "id": "deep" } } ], "else": [ { "addUnit": { "character": "vex" } } ] },
                                                { "choice": {}, "answers": [ { "do": [ { "addUnit": { "id": "ans" } } ] } ] } ] } ] })");
  const DocLookup none;
  using V = std::vector<std::string>;
  CHECK(resolveIdSources(map, none, {"@sides"}) == (V{"blue", "red", "gold"}));
  CHECK(resolveIdSources(map, none, {"keys:@changes"}) == (V{"bridge", "gate"}));
  CHECK(resolveIdSources(map, none, {"@teams"}) == (V{"allies", "red", "blue", "gold"}));  // team names, then side ids not yet listed
  CHECK(resolveIdSources(map, none, {"@storyUnits"}) == (V{"a1", "eirik", "late", "kell", "deep", "vex", "ans"}));
  CHECK(storyUnitIds(map).size() == 7u);
  CHECK(teamIds(map).size() == 4u);
  // The file's own sources come first, then the other files' (the order forms have always shown).
  const Json chars = J(R"({ "characters": [ {"id": "eirik"}, {"id": "zed"} ] })");
  const DocLookup docs = [&](const std::string& rel) -> const Json* { return rel == "data/characters.json" ? &chars : nullptr; };
  CHECK(resolveIdSources(map, docs, {"data/characters.json", "@storyUnits"}) == (V{"a1", "eirik", "late", "kell", "deep", "vex", "ans", "zed"}));
}

TEST(schema_ids_table_array_shapes) {
  CHECK(tableArray(J(R"([ {"id":"a"} ])")) != nullptr);
  CHECK(tableArray(J(R"({ "units": [ {"id":"a"} ] })")) != nullptr);
  CHECK(tableArray(J(R"({ "units": [] })")) == nullptr);           // says nothing about its columns
  CHECK(tableArray(J(R"({ "a": [ {"id":"a"} ], "b": 1 })")) == nullptr);
  CHECK(tableArray(J(R"([ 1, 2 ])")) == nullptr);
}

TEST(schema_engine_variants_tell_arrays_apart_by_length_and_first_item) {
  SchemaSet set;
  set.addFromText("t.json", R"({ "files": ["t.json"], "root": { "fields": { "join": { "type": "variant", "variants": [
      { "name": "one block", "kind": "array", "first": "number", "sample": [0, 2], "node": { "type": "list", "length": 2, "of": { "type": "int" } } },
      { "name": "several", "kind": "array", "first": "array", "sample": [[0, 2]], "node": { "type": "list", "of": { "type": "list", "length": 2, "of": { "type": "int" } } } } ] },
    "frame": { "type": "variant", "variants": [
      { "name": "cell", "kind": "array", "length": 2, "node": { "type": "list", "length": 2, "of": { "type": "int" } } },
      { "name": "cell of a sheet", "kind": "array", "length": 3, "node": { "type": "list", "tuple": [ { "type": "int" }, { "type": "int" }, { "type": "ref", "ref": "@sheets" } ] } } ] } } } })");
  CHECK_STR(errorsOf(set), "");
  const SchemaNode& join = *set.forFile("t.json")->root->field("join");
  const SchemaNode& frame = *set.forFile("t.json")->root->field("frame");
  CHECK_EQ(variantIndex(join, J("[0,2]")), 0);
  CHECK_EQ(variantIndex(join, J("[[0,2],[1,2]]")), 1);
  CHECK_EQ(variantIndex(join, J("[]")), -1);  // no first item: neither shape
  CHECK_EQ(variantIndex(join, J("[\"x\"]")), -1);
  CHECK_EQ(variantIndex(join, J("\"x\"")), -1);
  CHECK_EQ(variantIndex(frame, J("[1,2]")), 0);
  CHECK_EQ(variantIndex(frame, J("[1,2,\"fx\"]")), 1);
  CHECK_EQ(variantIndex(frame, J("[1,2,3,4]")), -1);
  // A tuple: a fixed list with a node per position (the item node of position 2 is the sheet id); `of` stays unset.
  const SchemaNode& tuple = *frame.variants[1].node;
  CHECK_EQ(tuple.length, 3);
  CHECK(tuple.of == nullptr);
  CHECK(tuple.itemNode(0)->kind == SchemaNode::Kind::Int);
  CHECK(tuple.itemNode(2)->kind == SchemaNode::Kind::Ref);
  CHECK(tuple.itemNode(3) == nullptr);
  CHECK(typeOk(tuple, J("[1,2,\"fx\"]")));
  CHECK(!typeOk(tuple, J("[1,2]")));
  const IdLookup ids = [](const std::vector<std::string>&) { return std::vector<std::string>{"sheetA"}; };
  CHECK_STR(schemaDefault(tuple, ids).dump(0), "[0,0,\"sheetA\"]");
  CHECK_STR(schemaSample(tuple, ids).dump(0), "[1,1,\"sheetA\"]");
  // "length" and "first" belong to kind array; "tuple" to a list.
  SchemaSet bad;
  bad.addFromText("b.json", R"({ "files": ["b.json"], "root": { "fields": {
      "v": { "type": "variant", "variants": [ { "name": "x", "kind": "string", "length": 2, "node": { "type": "string" } } ] },
      "t": { "type": "string", "tuple": [ { "type": "int" } ] } } } })");
  CHECK(contains(errorsOf(bad), "\"length\" and \"first\" go with"));
  CHECK(contains(errorsOf(bad), "\"tuple\" belongs to a list"));
}

TEST(schema_engine_enable_starts_any_node_and_null_can_be_an_alternative) {
  SchemaSet set;
  set.addFromText("t.json", R"({ "files": ["t.json"], "root": { "fields": {
      "frames": { "type": "list", "required": true, "enable": [[0, 0]], "sample": [[5, 5]], "of": { "type": "list", "length": 2, "of": { "type": "int" } } },
      "anim": { "type": "object", "alt": "null", "block": true, "fields": { "frames": { "type": "list", "required": true, "enable": [[0, 0]], "of": { "type": "list", "length": 2, "of": { "type": "int" } } } } },
      "step": { "type": "object", "enable": { "from": 2, "delayMs": 100 }, "fields": { "from": { "type": "int", "required": true, "min": 1 } } } } } })");
  CHECK_STR(errorsOf(set), "");
  const SchemaNode& root = *set.forFile("t.json")->root;
  CHECK_STR(schemaDefault(*root.field("frames"), nullptr).dump(0), "[[0,0]]");  // a required list does not start empty
  CHECK_STR(schemaSample(*root.field("frames"), nullptr).dump(0), "[[5,5]]");
  CHECK_STR(schemaDefault(*root.field("anim"), nullptr).dump(0), "{\"frames\":[[0,0]]}");
  CHECK_STR(schemaDefault(*root.field("step"), nullptr).dump(0), "{\"from\":2,\"delayMs\":100}");  // what a new list item starts with
  CHECK(isAlt(*root.field("anim"), J("null")));
  CHECK(typeOk(*root.field("anim"), J("null")));
  CHECK(!isAlt(*root.field("anim"), J("1")));
}

TEST(schema_engine_the_real_schemas_load_clean) {
  SchemaSet set;
  set.loadFolder(std::string(OTE_SOURCE_DIR) + "/editor/schema");
  CHECK_STR(errorsOf(set), "");
  for (const char* f : {"game.json", "data/weapons.json", "data/units.json", "data/terrain.json", "data/items.json", "maps/field.json", "data/skills.json", "data/campaign.json"})
    CHECK(set.forFile(f) != nullptr);
  SchemaSet again;
  again.loadFolder(std::string(OTE_SOURCE_DIR) + "/editor/schema_does_not_exist");
  CHECK(again.all().empty());
}
