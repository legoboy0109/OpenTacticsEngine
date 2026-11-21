// The tables of the schema-vs-loader test (test_conformance.cpp): which fixture each schema key is tried in, which edits make
// that fixture suit the key, special samples, and every reason a key or value is NOT tested. Data only, split by file family
// (conformance_core.cpp, conformance_srpg.cpp, conformance_story.cpp) so each stays small.
#pragma once
#include <QString>
#include <map>
#include <vector>

namespace conformance {

struct Op {
  QString file;  // project-relative
  QString path;  // dotted, with [n]: rules.actionPoints, jobs[2].learn; "[+]" appends to a list
  QString json;  // the new value; "-" removes the key
};

struct Context {
  const char* name;
  const char* fixture;
  std::vector<Op> mods;                  // edits that make the fixture suit the keys below
  std::vector<const char*> claims;       // "schema:key.path" prefixes this context tests (first matching context wins)
  std::map<QString, QString> overrides;  // "schema:key.path" -> sample JSON when the generated one would not do
};

// Keys are written "<schema>:<dotted path>"; list items "[]", a variant's shape "{name}", the entries of a map "*":
// "game:rules.ranks.levels[].at", "skills:blocks[].if{hp}.hp.op", "game:rules.ai.weights.*.dmg".
const std::vector<Context>& contexts();
// Keys that are not sampled at all, with the reason.
const std::map<QString, QString>& skipped();
// "key = value" the loader legitimately refuses in the context (another enum word, the end of a range, an invalid value it
// does not refuse at load time), with the reason.
const std::map<QString, QString>& extraSkips();
// Blocks whose "Enable" alone cannot be valid, with the reason ("schema:path").
const std::map<QString, QString>& blockExcuses();
// Keys of fixtures that sit in a section the schema deliberately does not describe. True and `why` set when excused.
bool excusedKey(const QString& schema, const QString& path, QString* why);

// Family tables, appended to the list in this order.
void addCoreContexts(std::vector<Context>& out);
void addGameContexts(std::vector<Context>& out);
void addDataContexts(std::vector<Context>& out);
void addSrpgContexts(std::vector<Context>& out);
void addStoryContexts(std::vector<Context>& out);
void addCoreExtraSkips(std::map<QString, QString>& out);
void addGameExtraSkips(std::map<QString, QString>& out);
void addDataExtraSkips(std::map<QString, QString>& out);
void addSrpgExtraSkips(std::map<QString, QString>& out);
void addStoryExtraSkips(std::map<QString, QString>& out);

}  // namespace conformance
